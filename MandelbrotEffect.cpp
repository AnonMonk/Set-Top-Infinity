#include "MandelbrotEffect.h"

#include <cmath>

#include "vipgfx.h"

namespace
{
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
		float margin = (1.0f - crop) * 0.5f;
		float u0 = margin;
		float v0 = margin;
		float u1 = 1.0f - margin;
		float v1 = 1.0f - margin;

		glBindTexture(GL_TEXTURE_2D, texture);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glBegin(GL_QUADS);
			glTexCoord2f(u0, v1); glVertex2f(-1.0f, -1.0f);
			glTexCoord2f(u1, v1); glVertex2f( 1.0f, -1.0f);
			glTexCoord2f(u1, v0); glVertex2f( 1.0f,  1.0f);
			glTexCoord2f(u0, v0); glVertex2f(-1.0f,  1.0f);
		glEnd();
	}
}

void drawMandelbrotEffect(
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

	float progress = clamp01(animTime / duration);
	float levelPosition = progress * (float)(textureCount - 1);
	int level = (int)levelPosition;
	if (level >= textureCount)
		level = textureCount - 1;

	float levelProgress = levelPosition - (float)level;
	float crop = powf(0.64f, levelProgress);
	float fadeIn = smoothStep(animTime / 0.55f);
	float fadeOut =
		1.0f - smoothStep((animTime - (duration - 0.70f)) / 0.65f);
	float sceneAlpha = fadeIn * fadeOut;

	drawZoomTexture(textures[level], crop, sceneAlpha);

	if (level + 1 < textureCount)
	{
		float nextAlpha =
			smoothStep((levelProgress - 0.88f) / 0.12f) * sceneAlpha;
		drawZoomTexture(textures[level + 1], 1.0f, nextAlpha);
	}

	glDisable(GL_BLEND);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
