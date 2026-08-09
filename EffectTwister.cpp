#include "EffectTwister.h"

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

	float smoothTurn(float progress)
	{
		progress = clamp01(progress);
		return progress * progress * progress
			* (progress * (progress * 6.0f - 15.0f) + 10.0f);
	}

	void twisterPoint(
		float y,
		float motionTime,
		float rotationPhase,
		int corner,
		float* x,
		float* z
	)
	{
		float center =
			sinf(y * 2.3f - motionTime * 0.75f) * 0.22f
			+ sinf(y * 5.1f + motionTime * 0.43f) * 0.045f;

		float radius =
			0.36f * (1.0f + 0.10f * sinf(motionTime * 1.2f - y * 3.0f));

		float phase =
			rotationPhase
			+ y * 6.2f
			+ sinf(y * 1.1f) * 0.45f;

		float angle = phase + (float)corner * PI * 0.5f;

		*x = center + cosf(angle) * radius;
		*z = sinf(angle) * 0.45f;
	}

	void setTwisterColor(float z, float y, float time)
	{
		float depthLight = 0.38f + 0.62f * clamp01((z + 0.45f) / 0.90f);
		float light = depthLight;
		/*
		 * Die Farbe haengt nur von der Hoehe ab. Dadurch haben alle vier
		 * Flaechen an einer Kante exakt denselben Farbwert.
		 */
		float colorPhase = y * 1.8f - time * 0.35f;
		float red = 0.18f + 0.82f * (0.5f + 0.5f * sinf(colorPhase));
		float green =
			0.18f + 0.82f * (0.5f + 0.5f * sinf(colorPhase + 2.0944f));
		float blue =
			0.18f + 0.82f * (0.5f + 0.5f * sinf(colorPhase + 4.1888f));

		glColor4f(
			red * light,
			green * light,
			blue * light,
			1.0f
		);
	}
}

void drawTwisterEffect(float animTime)
{
	int w = vscreen.width;
	int h = vscreen.height;
	float aspect = (h > 0) ? (float)w / (float)h : 1.3333f;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-aspect, aspect, -1.0, 1.0, -2.0, 2.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0.004f, 0.008f, 0.025f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	/* Ruhige Sinuslinien (weniger = billiger auf ATV) */
	glLineWidth(1.0f);
	const int bgLines = 8;
	const int bgSamples = 28;
	for (int line = 0; line < bgLines; line++)
	{
		float baseY = -1.0f + (float)line * (2.0f / (float)(bgLines - 1));
		float alpha = 0.035f + 0.025f * sinf(animTime + (float)line);

		glColor4f(0.10f, 0.45f, 0.80f, alpha);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i <= bgSamples; i++)
		{
			float x = -aspect + (2.0f * aspect * (float)i / (float)bgSamples);
			float y = baseY
				+ 0.018f * sinf(x * 5.0f + animTime * 1.4f + line * 0.7f);
			glVertex3f(x, y, 1.2f);
		}
		glEnd();
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	/* Weniger Segmente: weniger Quads + Kanten-Vertices */
	const int segments = 80;
	/*
	 * Laenger als die Bildschirmdiagonale: So reicht der Twister auch
	 * waehrend der Drehung bei 4:3 und 16:9 immer ueber die Raender.
	 */
	const float halfLength = sqrtf(aspect * aspect + 1.0f) + 0.40f;
	const float bottom = -halfLength;
	const float top = halfLength;
	const float rotationDelay = 3.0f;
	const float turnDuration = 6.0f;

	float rotationElapsed = animTime - rotationDelay;
	float wholeRotationDegrees = 0.0f;
	if (rotationElapsed > 0.0f)
	{
		if (rotationElapsed < turnDuration)
		{
			wholeRotationDegrees =
				-360.0f * smoothTurn(rotationElapsed / turnDuration);
		}
		else if (rotationElapsed < turnDuration * 2.0f)
		{
			wholeRotationDegrees =
				-360.0f
				+ 360.0f * smoothTurn(
					(rotationElapsed - turnDuration) / turnDuration
				);
		}
	}

	/*
	 * Nach drei Sekunden dreht sich die komplette Form um die
	 * Bildschirmmitte. Ein negativer Winkel ist hier im Uhrzeigersinn.
	 */
	glPushMatrix();
	glRotatef(wholeRotationDegrees, 0.0f, 0.0f, 1.0f);

	/* Vier gedrehte Flaechen bilden den klassischen Twister. */
	glBegin(GL_QUADS);
	for (int segment = 0; segment < segments; segment++)
	{
		float y0 = bottom + (top - bottom) * (float)segment / (float)segments;
		float y1 = bottom + (top - bottom) * (float)(segment + 1) / (float)segments;

		for (int face = 0; face < 4; face++)
		{
			int nextCorner = (face + 1) & 3;
			float x00, z00, x01, z01;
			float x10, z10, x11, z11;

			twisterPoint(y0, animTime, 0.0f, face, &x00, &z00);
			twisterPoint(y0, animTime, 0.0f, nextCorner, &x01, &z01);
			twisterPoint(y1, animTime, 0.0f, nextCorner, &x11, &z11);
			twisterPoint(y1, animTime, 0.0f, face, &x10, &z10);

			setTwisterColor(z00, y0, animTime);
			glVertex3f(x00, y0, z00);
			setTwisterColor(z01, y0, animTime);
			glVertex3f(x01, y0, z01);
			setTwisterColor(z11, y1, animTime);
			glVertex3f(x11, y1, z11);
			setTwisterColor(z10, y1, animTime);
			glVertex3f(x10, y1, z10);
		}
	}
	glEnd();

	/* Leuchtende Kanten lassen die Drehung klarer lesbar werden. */
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glLineWidth(2.0f);

	for (int corner = 0; corner < 4; corner++)
	{
		glBegin(GL_LINE_STRIP);
		for (int segment = 0; segment <= segments; segment++)
		{
			float y =
				bottom + (top - bottom) * (float)segment / (float)segments;
			float x, z;
			twisterPoint(
				y,
				animTime,
				0.0f,
				corner,
				&x,
				&z
			);

			float colorPhase = y * 1.8f - animTime * 0.35f;
			glColor4f(
				0.25f + 0.75f * (0.5f + 0.5f * sinf(colorPhase)),
				0.25f + 0.75f
					* (0.5f + 0.5f * sinf(colorPhase + 2.0944f)),
				0.25f + 0.75f
					* (0.5f + 0.5f * sinf(colorPhase + 4.1888f)),
				0.32f
			);
			glVertex3f(x, y, z);
		}
		glEnd();
	}

	glPopMatrix();

	glLineWidth(1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
