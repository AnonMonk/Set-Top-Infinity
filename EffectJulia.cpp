#include "EffectJulia.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>

#include "gettime.h"

#ifdef __linux__
#include <GL/gl.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#endif

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif

namespace
{
	const int kWidth = 384;
	const int kHeight = 216;
	const float kZoomDepth = 16.0f;
	const float kJuliaCRe = -0.8f;
	const float kJuliaCIm = 0.156f;
	// This preimage stays on the Julia boundary throughout the deep zoom.
	const float kZoomTargetX = -0.2059743933f;
	const float kZoomTargetY = -0.0177253464f;
	const float kStartSpan = 3.0f;
	const float kZoomFactor = 0.64f;

	const int kMaxPaletteIteration = 88;
	const int kPaletteSubsteps = 32;
	const int kPaletteEntryCount =
		(kMaxPaletteIteration + 2) * kPaletteSubsteps;
	const int kRadiusTableSize = 512;
	const float kEscapeRadiusSquaredMin = 4.0f;
	const float kEscapeRadiusSquaredMax = 25.0f;

	GLuint gTexture = 0;
	uint16_t* gPixels = 0;
	uint16_t gPalette565[kPaletteEntryCount];
	uint16_t gInterior565 = 0;
	float gSmoothCorrection[kRadiusTableSize];
	bool gInited = false;
	float gLastAnimTime = -1.0f;
	uint64_t gStatsMicroseconds = 0;
	int gStatsFrames = 0;
	int gEscapedPixels = 0;

	float clamp01(float value)
	{
		if (value < 0.0f) return 0.0f;
		if (value > 1.0f) return 1.0f;
		return value;
	}

	float smoothStep(float value)
	{
		value = clamp01(value);
		return value * value * (3.0f - 2.0f * value);
	}

	uint16_t pack565(float r, float g, float b)
	{
		if (r < 0.0f) r = 0.0f;
		if (g < 0.0f) g = 0.0f;
		if (b < 0.0f) b = 0.0f;
		if (r > 1.0f) r = 1.0f;
		if (g > 1.0f) g = 1.0f;
		if (b > 1.0f) b = 1.0f;
		const unsigned red = (unsigned)(r * 31.0f + 0.5f);
		const unsigned green = (unsigned)(g * 63.0f + 0.5f);
		const unsigned blue = (unsigned)(b * 31.0f + 0.5f);
		return (uint16_t)((red << 11) | (green << 5) | blue);
	}

	void initColorTables()
	{
		for (int i = 0; i < kPaletteEntryCount; ++i)
		{
			const float smooth = (float)i / (float)kPaletteSubsteps;
			const float t = smooth * 0.31f;
			const float w0 = 0.5f + 0.5f * cosf(t);
			const float w1 = 0.5f + 0.5f * cosf(t + 2.1f);
			const float w2 = 0.5f + 0.5f * cosf(t + 4.2f);
			float brightness = smooth / 16.0f;
			if (brightness < 0.22f) brightness = 0.22f;
			if (brightness > 1.0f) brightness = 1.0f;
			gPalette565[i] = pack565(
				(0.20f + 0.70f * w0 + 0.25f * w2) * brightness,
				(0.04f + 0.22f * w1 + 0.35f * w2) * brightness,
				(0.18f + 0.55f * w1 + 0.15f * w0) * brightness
			);
		}
		gInterior565 = pack565(8.0f / 255.0f, 2.0f / 255.0f, 14.0f / 255.0f);

		for (int i = 0; i < kRadiusTableSize; ++i)
		{
			const float t = (float)i / (float)(kRadiusTableSize - 1);
			const float radiusSquared = kEscapeRadiusSquaredMin
				+ t * (kEscapeRadiusSquaredMax - kEscapeRadiusSquaredMin);
			const float logarithm = logf(sqrtf(radiusSquared));
			gSmoothCorrection[i] = log2f(logarithm > 1.0f ? logarithm : 1.0f);
		}
	}

	struct PixelResult
	{
		int iteration;
		float radiusSquared;
		bool escaped;
	};

#if !defined(__SSE__)
	PixelResult iterateScalar(float zx, float zy, int maxIter)
	{
		PixelResult result = { 0, 0.0f, false };
		for (int iter = 0; iter < maxIter; ++iter)
		{
			const float zx2 = zx * zx;
			const float zy2 = zy * zy;
			const float radiusSquared = zx2 + zy2;
			if (radiusSquared > 4.0f)
			{
				result.iteration = iter;
				result.radiusSquared = radiusSquared;
				result.escaped = true;
				break;
			}
			const float nextX = zx2 - zy2 + kJuliaCRe;
			zy = 2.0f * zx * zy + kJuliaCIm;
			zx = nextX;
		}
		return result;
	}
#endif

	void iterateFour(
		float firstX,
		float stepX,
		float y,
		int maxIter,
		int count,
		PixelResult* results
	)
	{
#if defined(__SSE__)
		__m128 zx = _mm_set_ps(
			firstX + 3.0f * stepX,
			firstX + 2.0f * stepX,
			firstX + stepX,
			firstX
		);
		__m128 zy = _mm_set1_ps(y);
		const __m128 cRe = _mm_set1_ps(kJuliaCRe);
		const __m128 cIm = _mm_set1_ps(kJuliaCIm);
		const __m128 four = _mm_set1_ps(4.0f);
		__m128 active = _mm_cmpeq_ps(zx, zx);
		__m128 escapeIterations = _mm_set1_ps(-1.0f);
		__m128 escapeRadius = _mm_setzero_ps();

		for (int iter = 0; iter < maxIter; ++iter)
		{
			const __m128 zx2 = _mm_mul_ps(zx, zx);
			const __m128 zy2 = _mm_mul_ps(zy, zy);
			const __m128 radius = _mm_add_ps(zx2, zy2);
			const __m128 escapedNow = _mm_and_ps(active, _mm_cmpgt_ps(radius, four));
			const __m128 iterValue = _mm_set1_ps((float)iter);
			escapeIterations = _mm_or_ps(
				_mm_and_ps(escapedNow, iterValue),
				_mm_andnot_ps(escapedNow, escapeIterations)
			);
			escapeRadius = _mm_or_ps(
				_mm_and_ps(escapedNow, radius),
				_mm_andnot_ps(escapedNow, escapeRadius)
			);
			active = _mm_andnot_ps(escapedNow, active);
			if (_mm_movemask_ps(active) == 0)
				break;

			const __m128 nextX = _mm_add_ps(_mm_sub_ps(zx2, zy2), cRe);
			zy = _mm_add_ps(_mm_mul_ps(_mm_add_ps(zx, zx), zy), cIm);
			zx = nextX;
		}

		float iterations[4];
		float radii[4];
		_mm_storeu_ps(iterations, escapeIterations);
		_mm_storeu_ps(radii, escapeRadius);
		for (int lane = 0; lane < count; ++lane)
		{
			results[lane].escaped = iterations[lane] >= 0.0f;
			results[lane].iteration = results[lane].escaped
				? (int)iterations[lane] : 0;
			results[lane].radiusSquared = results[lane].escaped
				? radii[lane] : 0.0f;
		}
#else
		for (int lane = 0; lane < count; ++lane)
			results[lane] = iterateScalar(firstX + (float)lane * stepX, y, maxIter);
#endif
	}

	uint16_t colorFromResult(const PixelResult& result)
	{
		if (!result.escaped)
			return gInterior565;

		const float radiusScale = (float)(kRadiusTableSize - 1)
			/ (kEscapeRadiusSquaredMax - kEscapeRadiusSquaredMin);
		int radiusIndex = (int)(
			(result.radiusSquared - kEscapeRadiusSquaredMin) * radiusScale + 0.5f
		);
		if (radiusIndex < 0) radiusIndex = 0;
		if (radiusIndex >= kRadiusTableSize) radiusIndex = kRadiusTableSize - 1;

		const float smooth = (float)result.iteration + 1.0f
			- gSmoothCorrection[radiusIndex];
		int paletteIndex = (int)(smooth * (float)kPaletteSubsteps + 0.5f);
		if (paletteIndex < 0) paletteIndex = 0;
		if (paletteIndex >= kPaletteEntryCount)
			paletteIndex = kPaletteEntryCount - 1;
		return gPalette565[paletteIndex];
	}

	void viewParams(
		float progress,
		float* x0,
		float* y0,
		float* stepX,
		float* stepY,
		int* maxIter
	)
	{
		const float depth = clamp01(progress) * kZoomDepth;
		const float spanX = kStartSpan * powf(kZoomFactor, depth);
		const float spanY = spanX * (float)kHeight / (float)kWidth;
		const float cameraMove = smoothStep(progress / 0.35f);
		const float centerX = kZoomTargetX * cameraMove;
		const float centerY = kZoomTargetY * cameraMove;
		*x0 = centerX - spanX * 0.5f;
		*y0 = centerY - spanY * 0.5f;
		*stepX = spanX / (float)(kWidth - 1);
		*stepY = spanY / (float)(kHeight - 1);
		*maxIter = 72 + (int)(depth + 0.5f);
		if (*maxIter > kMaxPaletteIteration)
			*maxIter = kMaxPaletteIteration;
	}

	int renderFrame(float progress)
	{
		float x0, y0, stepX, stepY;
		int maxIter;
		viewParams(progress, &x0, &y0, &stepX, &stepY, &maxIter);
		gEscapedPixels = 0;

		for (int py = 0; py < kHeight; ++py)
		{
			uint16_t* row = gPixels + (size_t)py * (size_t)kWidth;
			const float y = y0 + (float)py * stepY;
			for (int px = 0; px < kWidth; px += 4)
			{
				const int remaining = kWidth - px;
				const int count = remaining < 4 ? remaining : 4;
				PixelResult results[4];
				iterateFour(
					x0 + (float)px * stepX,
					stepX,
					y,
					maxIter,
					count,
					results
				);
				for (int lane = 0; lane < count; ++lane)
				{
					row[px + lane] = colorFromResult(results[lane]);
					if (results[lane].escaped)
						++gEscapedPixels;
				}
			}
		}
		return maxIter;
	}

	void uploadFrame()
	{
		glBindTexture(GL_TEXTURE_2D, gTexture);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(
			GL_TEXTURE_2D, 0, 0, 0,
			kWidth, kHeight,
			GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
			gPixels
		);
	}

	void drawTexture(float alpha)
	{
		glBindTexture(GL_TEXTURE_2D, gTexture);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
			glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
			glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
		glEnd();
	}
}

static void initJuliaLiveRenderer();

void drawJuliaEffect(float animTime, float duration)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (duration <= 0.0f)
		return;
	if (!gInited)
		initJuliaLiveRenderer();

	if (gLastAnimTime < 0.0f || fabsf(animTime - gLastAnimTime) >= 0.0001f)
	{
		const uint64_t start = GetMicroseconds();
		const float progress = clamp01(animTime / duration);
		const int maxIter = renderFrame(progress);
		uploadFrame();
		gLastAnimTime = animTime;
		gStatsMicroseconds += GetMicroseconds() - start;
		++gStatsFrames;

		if (gStatsFrames >= 60)
		{
			printf(
				"[Julia CPU live] Zoom %5.1f%%, Iter %d, "
				"CPU %.2f ms/frame, SSE-Pixel %d, sichtbar %.1f%%\n",
				progress * 100.0f,
				maxIter,
				(double)gStatsMicroseconds / (1000.0 * (double)gStatsFrames),
				kWidth * kHeight,
				100.0 * (double)gEscapedPixels
					/ (double)(kWidth * kHeight)
			);
			fflush(stdout);
			gStatsMicroseconds = 0;
			gStatsFrames = 0;
		}
	}

	const float fadeIn = smoothStep(animTime / 0.55f);
	const float fadeOut =
		1.0f - smoothStep((animTime - (duration - 0.70f)) / 0.65f);
	drawTexture(fadeIn * fadeOut);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

static void initJuliaLiveRenderer()
{
	if (gInited)
		return;

	initColorTables();
	const size_t pixelCount = (size_t)kWidth * (size_t)kHeight;
	gPixels = (uint16_t*)malloc(pixelCount * sizeof(uint16_t));
	if (!gPixels)
	{
		printf("Julia CPU live: OOM (%zu bytes)\n", pixelCount * sizeof(uint16_t));
		fflush(stdout);
		exit(1);
	}
	for (size_t i = 0; i < pixelCount; ++i)
		gPixels[i] = gInterior565;

	glGenTextures(1, &gTexture);
	glBindTexture(GL_TEXTURE_2D, gTexture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGB5,
		kWidth, kHeight, 0,
		GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
		gPixels
	);
	gLastAnimTime = -1.0f;
	gStatsMicroseconds = 0;
	gStatsFrames = 0;
	gInited = true;
	printf(
		"[Julia CPU live] START c=(%.3f, %.3f), %s, %dx%d, RGB565\n",
		kJuliaCRe,
		kJuliaCIm,
#if defined(__SSE__)
		"SSE",
#else
		"scalar",
#endif
		kWidth,
		kHeight
	);
	fflush(stdout);
}

void juliaLiveShutdown()
{
	if (!gInited)
		return;
	if (gTexture)
	{
		glDeleteTextures(1, &gTexture);
		gTexture = 0;
	}
	free(gPixels);
	gPixels = 0;
	gLastAnimTime = -1.0f;
	gInited = false;
}
