#include "EffectJulia.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

#include "vipgfx.h"

namespace
{
	/* Julia: fester c-Parameter (klassischer Doppelspiral-Look) */
	const double kJuliaCRe = -0.8;
	const double kJuliaCIm = 0.156;
	/* Zoom-Center in der z-Ebene (0,0 = Mitte der Menge) */
	const double kCenterX = 0.0;
	const double kCenterY = 0.0;
	const double kStartSpan = 3.0;
	const double kZoomFactor = 0.64;

	std::uint64_t nowMs()
	{
		using namespace std::chrono;
		return (std::uint64_t)duration_cast<milliseconds>(
			steady_clock::now().time_since_epoch()
		).count();
	}

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

	void drawZoomTexture(GLuint texture, float crop, float alpha)
	{
		const float margin = (1.0f - crop) * 0.5f;
		const float u0 = margin;
		const float v0 = margin;
		const float u1 = 1.0f - margin;
		const float v1 = 1.0f - margin;

		glBindTexture(GL_TEXTURE_2D, texture);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glBegin(GL_QUADS);
			glTexCoord2f(u0, v1); glVertex2f(-1.0f, -1.0f);
			glTexCoord2f(u1, v1); glVertex2f( 1.0f, -1.0f);
			glTexCoord2f(u1, v0); glVertex2f( 1.0f,  1.0f);
			glTexCoord2f(u0, v0); glVertex2f(-1.0f,  1.0f);
		glEnd();
	}

	/* levelIndex 0..COUNT-1 → continuous depth 0..ZOOM_DEPTH (same total as 20 old levels) */
	double levelDepth(int levelIndex)
	{
		if (JULIA_TEXTURE_COUNT <= 1)
			return 0.0;
		return (double)levelIndex * JULIA_ZOOM_DEPTH
			/ (double)(JULIA_TEXTURE_COUNT - 1);
	}

	/* Crop: Zoom in current level so center matches next level at end of step */
	float cropBetweenFineLevels(float levelProgress)
	{
		const double stepDepth =
			JULIA_ZOOM_DEPTH / (double)(JULIA_TEXTURE_COUNT - 1);
		return (float)pow(kZoomFactor, (double)levelProgress * stepDepth);
	}

	/*
	 * Weicher Stufenwechsel:
	 * - Crossfade startet frueh (nicht erst bei 88%) und laeuft lang
	 * - Aktuelle Stufe fadet raus, naechste rein (echtes Crossfade)
	 * - Crop etwas geased, damit der Digital-Zoom weniger "linear-hart" wirkt
	 */
	/* Frueher als 0.88, aber nicht zu frueh (Doppelbild); ~halbe Stufe blenden */
	const float kCrossfadeStart = 0.45f;

	GLuint gTextures[JULIA_TEXTURE_COUNT];
	GLuint* gOutTextures = 0;
	unsigned char* gBuffer = 0;
	int gLevel = 0;
	int gNextRow = 0;
	int gLevelsDone = 0;
	bool gReady = false;
	bool gInited = false;
	bool gLoggedReady = false;
	std::uint64_t gPrecalcStartMs = 0;
	int gLastProgressPct = -1;

	void levelParams(int level, double* x0, double* y0, double* spanX, double* spanY, int* maxIter)
	{
		const double depth = levelDepth(level);
		const double sx = kStartSpan * pow(kZoomFactor, depth);
		const double sy = sx * (double)JULIA_HEIGHT / (double)JULIA_WIDTH;
		*spanX = sx;
		*spanY = sy;
		*x0 = kCenterX - sx * 0.5;
		*y0 = kCenterY - sy * 0.5;
		/* Weniger Iterationen als Full-Quality — Bigscreen noch ok */
		*maxIter = 90 + (int)(depth * 9.0 + 0.5);
		if (*maxIter < 64)
			*maxIter = 64;
	}

	void colorFromSmooth(double smooth, bool escaped, unsigned char* rgba)
	{
		if (!escaped)
		{
			/* Set-Inneres: tiefes Violett-Schwarz, nicht kalt-blau */
			rgba[0] = 8;
			rgba[1] = 2;
			rgba[2] = 14;
			rgba[3] = 255;
			return;
		}

		/*
		 * Julia-Palette: Magenta / Violett → Pink → warmes Gold
		 * (bewusst nicht cyan/blau — Demo hat schon blaue Szenen)
		 */
		const double t = smooth * 0.31;
		const double w0 = 0.5 + 0.5 * cos(t);
		const double w1 = 0.5 + 0.5 * cos(t + 2.1);
		const double w2 = 0.5 + 0.5 * cos(t + 4.2);

		double brightness = smooth / 16.0;
		if (brightness < 0.22) brightness = 0.22;
		if (brightness > 1.0) brightness = 1.0;

		/* Basis: kräftiges Magenta, Akzente Gold/Orange */
		double r = 0.20 + 0.70 * w0 + 0.25 * w2;
		double g = 0.04 + 0.22 * w1 + 0.35 * w2;
		double b = 0.18 + 0.55 * w1 + 0.15 * w0;

		r *= brightness;
		g *= brightness;
		b *= brightness;

		if (r < 0.0) r = 0.0;
		if (g < 0.0) g = 0.0;
		if (b < 0.0) b = 0.0;
		if (r > 1.0) r = 1.0;
		if (g > 1.0) g = 1.0;
		if (b > 1.0) b = 1.0;

		rgba[0] = (unsigned char)(r * 255.0);
		rgba[1] = (unsigned char)(g * 255.0);
		rgba[2] = (unsigned char)(b * 255.0);
		rgba[3] = 255;
	}

	void renderRow(int level, int py)
	{
		double x0, y0, spanX, spanY;
		int maxIter;
		levelParams(level, &x0, &y0, &spanX, &spanY, &maxIter);

		const double invW =
			(JULIA_WIDTH > 1) ? (1.0 / (double)(JULIA_WIDTH - 1)) : 0.0;
		const double invH =
			(JULIA_HEIGHT > 1) ? (1.0 / (double)(JULIA_HEIGHT - 1)) : 0.0;
		const double cy = y0 + ((double)py * invH) * spanY;
		unsigned char* row =
			gBuffer + (size_t)py * (size_t)JULIA_WIDTH * 4u;

		for (int px = 0; px < JULIA_WIDTH; ++px)
		{
			/* Julia: z0 = Pixel, c fest */
			double zx = x0 + ((double)px * invW) * spanX;
			double zy = cy;
			bool escaped = false;
			double smooth = 0.0;

			for (int iter = 0; iter < maxIter; ++iter)
			{
				const double zx2 = zx * zx;
				const double zy2 = zy * zy;
				if (zx2 + zy2 > 4.0)
				{
					const double absv = sqrt(zx2 + zy2);
					const double lg = log(absv);
					smooth = (double)iter + 1.0 - log2(lg > 1.0 ? lg : 1.0);
					escaped = true;
					break;
				}
				const double nzx = zx2 - zy2 + kJuliaCRe;
				zy = 2.0 * zx * zy + kJuliaCIm;
				zx = nzx;
			}

			colorFromSmooth(smooth, escaped, row + px * 4);
		}
	}

	void uploadLevel(int level)
	{
		glBindTexture(GL_TEXTURE_2D, gTextures[level]);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			JULIA_WIDTH,
			JULIA_HEIGHT,
			0,
			GL_RGBA,
			GL_UNSIGNED_BYTE,
			gBuffer
		);
		if (gOutTextures)
			gOutTextures[level] = gTextures[level];
	}

	void advanceLevel()
	{
		uploadLevel(gLevel);
		++gLevelsDone;
		++gLevel;
		gNextRow = 0;

		/* Fortschritt im Terminal (alle 10 %) */
		if (JULIA_TEXTURE_COUNT > 0)
		{
			const int pct =
				(gLevelsDone * 100) / JULIA_TEXTURE_COUNT;
			const int bucket = (pct / 10) * 10;
			if (bucket != gLastProgressPct && bucket < 100)
			{
				gLastProgressPct = bucket;
				printf(
					"[Julia precalc] %3d%%  (%d/%d Stufen)\n",
					bucket,
					gLevelsDone,
					JULIA_TEXTURE_COUNT
				);
				fflush(stdout);
			}
		}

		if (gLevel >= JULIA_TEXTURE_COUNT)
		{
			gReady = true;
			if (!gLoggedReady)
			{
				const std::uint64_t dt =
					(gPrecalcStartMs > 0) ? (nowMs() - gPrecalcStartMs) : 0;
				printf(
					"========================================\n"
					"  JULIA PRECALC FERTIG\n"
					"  c=(%.3f, %.3f)  %d Stufen  %dx%d  in %llu ms\n"
					"========================================\n",
					kJuliaCRe,
					kJuliaCIm,
					JULIA_TEXTURE_COUNT,
					JULIA_WIDTH,
					JULIA_HEIGHT,
					(unsigned long long)dt
				);
				fflush(stdout);
				gLoggedReady = true;
			}
		}
	}

	void createPlaceholders()
	{
		unsigned char black[4] = { 0, 0, 0, 255 };
		glGenTextures(JULIA_TEXTURE_COUNT, gTextures);
		for (int i = 0; i < JULIA_TEXTURE_COUNT; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, gTextures[i]);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexImage2D(
				GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, black
			);
			if (gOutTextures)
				gOutTextures[i] = gTextures[i];
		}
	}
}

void drawJuliaEffect(
	const GLuint* textures,
	int textureCount,
	float animTime,
	float duration
)
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

	if (textureCount <= 0 || duration <= 0.0f)
		return;

	int usable = juliaPrecalcLevelsDone();
	if (usable < 1)
		usable = 1;
	if (usable > textureCount)
		usable = textureCount;

	const float progress = clamp01(animTime / duration);
	const float levelPosition = progress * (float)(usable - 1);
	int level = (int)levelPosition;
	if (level >= usable)
		level = usable - 1;

	const float levelProgress = levelPosition - (float)level;

	/*
	 * Crop linear in levelProgress (geometrisch korrekt zum naechsten Level).
	 * Crossfade: NUR die naechste Stufe einblenden — aktuelle bleibt deckend.
	 * (Beides mit (1-t)/t auszublenden verdunkelt bei Standard-Alpha-Blend!)
	 */
	const float crop = cropBetweenFineLevels(levelProgress);

	const float fadeIn = smoothStep(animTime / 0.55f);
	const float fadeOut =
		1.0f - smoothStep((animTime - (duration - 0.70f)) / 0.65f);
	const float sceneAlpha = fadeIn * fadeOut;

	float blend = 0.0f;
	if (level + 1 < usable)
	{
		const float span = 1.0f - kCrossfadeStart;
		if (span > 1e-5f)
			blend = smoothStep((levelProgress - kCrossfadeStart) / span);
	}

	/* Basis: aktuelle Stufe, voller sceneAlpha, mit Crop-Zoom */
	drawZoomTexture(textures[level], crop, sceneAlpha);

	/* Darueber: naechste Stufe einblenden (echtes Lerp bei SRC_ALPHA/1-SRC) */
	if (level + 1 < usable && blend > 0.001f)
		drawZoomTexture(textures[level + 1], 1.0f, sceneAlpha * blend);

	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void juliaPrecalcInit(GLuint* outTextures)
{
	if (gInited)
		return;

	gOutTextures = outTextures;
	gLevel = 0;
	gNextRow = 0;
	gLevelsDone = 0;
	gReady = false;
	gLoggedReady = false;
	gLastProgressPct = -1;
	gPrecalcStartMs = nowMs();

	const size_t bytes =
		(size_t)JULIA_WIDTH * (size_t)JULIA_HEIGHT * 4u;
	gBuffer = (unsigned char*)malloc(bytes);
	if (!gBuffer)
	{
		printf("Julia precalc: OOM (%zu bytes)\n", bytes);
		fflush(stdout);
		exit(1);
	}
	memset(gBuffer, 0, bytes);

	createPlaceholders();
	gInited = true;

	printf(
		"[Julia precalc] START  c=(%.3f, %.3f)  %d Stufen  %dx%d\n",
		kJuliaCRe,
		kJuliaCIm,
		JULIA_TEXTURE_COUNT,
		JULIA_WIDTH,
		JULIA_HEIGHT
	);
	fflush(stdout);
}

void juliaPrecalcTick(double maxMs)
{
	if (!gInited || gReady || maxMs <= 0.0)
		return;

	const std::uint64_t t0 = nowMs();
	while (!gReady)
	{
		if ((double)(nowMs() - t0) >= maxMs)
			break;
		renderRow(gLevel, gNextRow);
		++gNextRow;
		if (gNextRow >= JULIA_HEIGHT)
			advanceLevel();
	}
}

void juliaPrecalcFinishBlocking()
{
	if (!gInited || gReady)
		return;

	const std::uint64_t t0 = nowMs();
	printf(
		"[Julia precalc] blocking (done %d/%d)…\n",
		gLevelsDone,
		JULIA_TEXTURE_COUNT
	);
	fflush(stdout);

	while (!gReady)
	{
		renderRow(gLevel, gNextRow);
		++gNextRow;
		if (gNextRow >= JULIA_HEIGHT)
			advanceLevel();
	}

	printf(
		"[Julia precalc] blocking fertig in %llu ms\n",
		(unsigned long long)(nowMs() - t0)
	);
	fflush(stdout);
}

bool juliaPrecalcIsReady()
{
	return gReady;
}

int juliaPrecalcLevelsDone()
{
	return gLevelsDone;
}

void juliaPrecalcShutdown()
{
	if (!gInited)
		return;
	if (gBuffer)
	{
		free(gBuffer);
		gBuffer = 0;
	}
	gOutTextures = 0;
	gInited = false;
	gReady = false;
	gLevel = 0;
	gNextRow = 0;
	gLevelsDone = 0;
}
