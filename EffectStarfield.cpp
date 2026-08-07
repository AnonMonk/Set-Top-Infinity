#include "EffectStarfield.h"



#include <cmath>
#include "vipgfx.h"

namespace
{
	const float PI = 3.1415926f;

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

	float smoothTurn(float value)
	{
		value = clamp01(value);
		return value * value * value
			* (value * (value * 6.0f - 15.0f) + 10.0f);
	}

	float starfieldRotation(float time)
	{
		const float start = 2.0f;
		const float turnDuration = 4.0f;
		const float fullTurn = PI * 2.0f;

		if (time < start)
			return 0.0f;
		if (time < start + turnDuration)
			return fullTurn
				* smoothTurn((time - start) / turnDuration);
		if (time < start + turnDuration * 2.0f)
			return fullTurn
				- fullTurn * smoothTurn(
					(time - start - turnDuration) / turnDuration
				);
		if (time < start + turnDuration * 3.0f)
			return fullTurn
				* smoothTurn(
					(time - start - turnDuration * 2.0f) / turnDuration
				);
		return fullTurn;
	}

	float randomValue(unsigned int index, unsigned int channel)
	{
		unsigned int value =
			index * 747796405u + channel * 2891336453u + 277803737u;
		value ^= value >> 16;
		value *= 2246822519u;
		value ^= value >> 13;
		return (float)(value & 65535u) / 65535.0f;
	}

	float wrappedDepth(float base, float travel)
	{
		float phase = base - travel;
		phase -= floorf(phase);
		if (phase < 0.0f)
			phase += 1.0f;
		return 0.10f + phase * 0.90f;
	}

	void starColor(
		unsigned int index,
		float depth,
		float time,
		float alpha
	)
	{
		float phase =
			randomValue(index, 3u) * PI * 2.0f
			+ time * 0.22f
			+ depth * 1.5f;
		float red = 0.55f + 0.45f * sinf(phase);
		float green = 0.65f + 0.35f * sinf(phase + 2.0944f);
		float blue = 0.75f + 0.25f * sinf(phase + 4.1888f);

		glColor4f(red, green, blue, alpha);
	}

	void projectStar(
		unsigned int index,
		float depth,
		float spiralStrength,
		float rotationAngle,
		float gather,
		float aspect,
		float* x,
		float* y
	)
	{
		float baseX = (randomValue(index, 0u) * 2.0f - 1.0f) * 0.92f;
		float baseY = (randomValue(index, 1u) * 2.0f - 1.0f) * 0.92f;
		float angle =
			rotationAngle
			+ spiralStrength * (1.0f - depth) * 1.8f;
		float ca = cosf(angle);
		float sa = sinf(angle);
		float rotatedX = baseX * ca - baseY * sa;
		float rotatedY = baseX * sa + baseY * ca;
		float perspective = 0.64f / depth;
		float gatherScale = 1.0f - gather;

		*x = rotatedX * perspective * aspect * gatherScale;
		*y = rotatedY * perspective * gatherScale;
	}

	void drawCenterGlow(float gather, float time, float aspect)
	{
		if (gather <= 0.55f)
			return;

		float strength = smoothStep((gather - 0.55f) / 0.45f);
		for (int ring = 0; ring < 4; ring++)
		{
			float radius =
				(0.025f + (float)ring * 0.025f)
				* (1.0f + 0.12f * sinf(time * 4.0f + ring));
			float alpha = strength * (0.16f - (float)ring * 0.025f);

			glColor4f(0.35f, 0.75f, 1.0f, alpha);
			glBegin(GL_LINE_LOOP);
			for (int i = 0; i < 48; i++)
			{
				float angle = (float)i / 48.0f * PI * 2.0f;
				glVertex2f(
					cosf(angle) * radius * aspect,
					sinf(angle) * radius
				);
			}
			glEnd();
		}
	}
}

void drawStarfieldEffect(float animTime, float duration)
{
	int w = vscreen.width;
	int h = vscreen.height;
	float aspect = (h > 0) ? (float)w / (float)h : 1.3333f;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-aspect, aspect, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0.002f, 0.004f, 0.015f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glShadeModel(GL_SMOOTH);

	float spiral = smoothStep((animTime - 2.0f) / 1.2f);
	float rotationAngle = starfieldRotation(animTime);
	float previousRotationAngle = starfieldRotation(animTime - 0.045f);
	float previousSpiral =
		smoothStep((animTime - 0.045f - 2.0f) / 1.2f);
	float gatherStart = duration - 4.0f;
	/* Erst im letzten Frame ganz zusammenfallen, ohne Schwarzblende. */
	float gather = smoothStep((animTime - gatherStart) / 4.0f);
	float fade = 1.0f;

	const int starCount = 360;
	const float speed = 0.27f;

	/* Leuchtspuren zeigen Flugrichtung und zunehmenden Wirbel. */
	glLineWidth(1.4f);
	glBegin(GL_LINES);
	for (int i = 0; i < starCount; i++)
	{
		unsigned int index = (unsigned int)i;
		float baseDepth = randomValue(index, 2u);
		float depth = wrappedDepth(baseDepth, animTime * speed);
		float previousDepth =
			wrappedDepth(baseDepth, animTime * speed - 0.012f);
		float x, y, previousX, previousY;

		/* Beim Zuruecksetzen in die Ferne keine Bildschirmdiagonale ziehen. */
		if (previousDepth < depth)
			previousDepth = depth;

		projectStar(
			index,
			previousDepth,
			previousSpiral,
			previousRotationAngle,
			gather,
			aspect,
			&previousX,
			&previousY
		);
		projectStar(
			index,
			depth,
			spiral,
			rotationAngle,
			gather,
			aspect,
			&x,
			&y
		);

		float brightness = clamp01((1.05f - depth) * 1.15f) * fade;
		starColor(index, depth, animTime, brightness * 0.10f);
		glVertex2f(previousX, previousY);
		starColor(index, depth, animTime, brightness * 0.72f);
		glVertex2f(x, y);
	}
	glEnd();

	/* Helle Koepfe auf den Spuren. */
	glPointSize(1.0f);
	glBegin(GL_POINTS);
	for (int i = 0; i < starCount; i++)
	{
		unsigned int index = (unsigned int)i;
		float depth =
			wrappedDepth(randomValue(index, 2u), animTime * speed);
		float x, y;
		projectStar(
			index,
			depth,
			spiral,
			rotationAngle,
			gather,
			aspect,
			&x,
			&y
		);

		float twinkle =
			0.72f + 0.28f
			* sinf(animTime * 5.0f + randomValue(index, 4u) * 20.0f);
		float brightness =
			clamp01((1.08f - depth) * 1.25f) * twinkle * fade;
		starColor(index, depth, animTime, brightness);
		glVertex2f(x, y);
	}
	glEnd();

	drawCenterGlow(gather, animTime, aspect);

	glPointSize(1.0f);
	glLineWidth(1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
