#include "EffectTunnel.h"

#include <cmath>
#include "vipgfx.h"

namespace
{
	const float PI = 3.1415926f;

	float tunnelWave(float time, float phase, float speed)
	{
		return 0.5f + 0.5f * sinf(time * speed + phase);
	}

	float slowedTunnelTime(float time)
	{
		const float normalDuration = 6.0f;
		const float slowDuration = 4.0f;

		if (time <= normalDuration)
			return time;

		float progress = (time - normalDuration) / slowDuration;
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		/*
		 * Integral einer weich von 1 auf 0 fallenden Geschwindigkeit.
		 * Position und Beschleunigung bleiben an beiden Enden stetig.
		 */
		float progress2 = progress * progress;
		float progress3 = progress2 * progress;
		float progress4 = progress3 * progress;
		return normalDuration
			+ slowDuration
			* (progress - progress3 + 0.5f * progress4);
	}
}

void drawTunnelEffect(GLuint texture, float animTime)
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int w = vscreen.width;
	int h = vscreen.height;

	float aspect = (h > 0) ? (float)w / (float)h : 1.3333f;
	float effectTime = slowedTunnelTime(animTime);
	float centerX = 0.18f * sinf(effectTime * 0.37f);
	float centerY = 0.14f * cosf(effectTime * 0.51f);

	

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);

	const int rings = 72;
	const int slices = 64;
	const float nearDepth = 0.55f;
	const float depthStep = 0.18f;

	for (int ring = 0; ring < rings - 1; ring++)
	{
		float d0 = nearDepth + (float)ring * depthStep;
		float d1 = nearDepth + (float)(ring + 1) * depthStep;

		float radius0 = 3.4f / d0;
		float radius1 = 3.4f / d1;

		float twist0 = effectTime * 0.8f + d0 * 0.35f;
		float twist1 = effectTime * 0.8f + d1 * 0.35f;

		float ringMix = (float)ring / (float)(rings - 1);
		float ringFade = 1.0f - ringMix * 0.55f;
		float v0 = d0 * 0.35f - effectTime * 1.6f;
		float v1 = d1 * 0.35f - effectTime * 1.6f;

		glBegin(GL_TRIANGLE_STRIP);
		for (int slice = 0; slice <= slices; slice++)
		{
			float t = (float)slice / (float)slices;
			float angle = t * PI * 2.0f;
			float u = t * 4.0f + effectTime * 0.18f;

			float colorPhase0 = angle * 2.0f + d0 * 0.8f;
			float colorPhase1 = angle * 2.0f + d1 * 0.8f;

			float x0 = centerX + cosf(angle + twist0) * radius0 / aspect;
			float y0 = centerY + sinf(angle + twist0) * radius0;
			float x1 = centerX + cosf(angle + twist1) * radius1 / aspect;
			float y1 = centerY + sinf(angle + twist1) * radius1;

			float r0 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase0, 0.70f);
			float g0 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase0 + 2.0f, 0.83f);
			float b0 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase0 + 4.0f, 0.97f);

			float r1 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase1, 0.70f);
			float g1 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase1 + 2.0f, 0.83f);
			float b1 = 0.30f + 0.70f * tunnelWave(effectTime, colorPhase1 + 4.0f, 0.97f);

			glColor4f(r0 * ringFade, g0 * ringFade, b0 * ringFade, 1.0f);
			glTexCoord2f(u, v0);
			glVertex2f(x0, y0);

			glColor4f(r1 * ringFade, g1 * ringFade, b1 * ringFade, 1.0f);
			glTexCoord2f(u, v1);
			glVertex2f(x1, y1);
		}
		glEnd();
	}

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
