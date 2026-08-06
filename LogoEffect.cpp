#include "LogoEffect.h"

#include "IntroAudioData.h"
#include "RotozoomEffect.h"

#include <cmath>
#include <cstdio>

#include "vipgfx.h"
#include "glTTF.h"

namespace
{

	aFont logoFont = {};


	struct LogoLayout
	{
		float headX;
		float headY;
		float headW;
		float headH;
		float tailX;
		float tailY;
		float tailW;
		float tailH;
		float bodyStartX;
		float bodyStartY;
		float bodyEndX;
		float bodyEndY;
	};

	void begin2D()
	{
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0, vscreen.width, vscreen.height, 0, -1, 1);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	void end2D()
	{
		glDisable(GL_BLEND);

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}

	void drawTexturedQuad(
		GLuint texture,
		float x,
		float y,
		float w,
		float h
	)
	{
		if (texture == 0) return;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		glBegin(GL_QUADS);
			glTexCoord2f(0.0f, 0.0f); glVertex2f(x,     y);
			glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
			glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(x,     y + h);
		glEnd();
	}

	void drawTexturedQuadWavy(
		GLuint texture,
		float x,
		float y,
		float w,
		float h,
		float alpha,
		float waveAmp,
		float animTime
	)
	{
		if (texture == 0) return;

		const int bands = 24;
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);

		for (int i = 0; i < bands; i++)
		{
			float t0 = (float)i / (float)bands;
			float t1 = (float)(i + 1) / (float)bands;
			float y0 = y + h * t0;
			float y1 = y + h * t1;
			float center = (t0 + t1) * 0.5f;
			float shift =
				sinf(animTime * 13.0f + center * 18.0f) * waveAmp;

			glColor4f(1.0f, 1.0f, 1.0f, alpha);
			glBegin(GL_QUADS);
				glTexCoord2f(0.0f, t0); glVertex2f(x + shift,     y0);
				glTexCoord2f(1.0f, t0); glVertex2f(x + w + shift, y0);
				glTexCoord2f(1.0f, t1); glVertex2f(x + w + shift, y1);
				glTexCoord2f(0.0f, t1); glVertex2f(x + shift,     y1);
			glEnd();
		}
	}

	float introAudioLevel(float seconds)
	{
		if (seconds < 0.0f || seconds >= INTRO_DURATION_SECONDS)
			return 0.0f;

		float position = seconds * INTRO_ENVELOPE_HZ;
		int index = (int)position;
		if (index < 0 || index >= INTRO_ENVELOPE_COUNT)
			return 0.0f;

		int next = index + 1;
		if (next >= INTRO_ENVELOPE_COUNT)
			next = index;

		float fraction = position - (float)index;
		return introEnvelope[index]
			+ (introEnvelope[next] - introEnvelope[index]) * fraction;
	}

	float fishOscSample(float t, float animTime)
	{
		float value = 0.0f;
		value += sinf(animTime * 5.0f + t * 18.0f) * 0.60f;
		value += sinf(animTime * 8.5f + t * 33.0f) * 0.25f;
		value += sinf(animTime * 2.0f + t * 11.0f) * 0.15f;

		float audioTime = animTime - t * 0.22f;
		float level = sqrtf(introAudioLevel(audioTime));
		return value * level;
	}

	void drawOscFishBody(
		float x0,
		float y0,
		float x1,
		float y1,
		float animTime
	)
	{
		const int samples = 96;
		const float amp = 42.0f;
		const float thickness = 13.0f;

		glDisable(GL_TEXTURE_2D);
		glColor4f(0.55f, 0.55f, 0.55f, 1.0f);

		glBegin(GL_TRIANGLE_STRIP);
		for (int i = 0; i < samples; i++)
		{
			float t = (float)i / (float)(samples - 1);
			float x = x0 + (x1 - x0) * t;
			float y = y0 + (y1 - y0) * t;
			float envelope = sinf(t * 3.1415926f);
			y += fishOscSample(t, animTime) * amp * envelope;
			float thick = thickness * envelope;

			glVertex2f(x, y - thick);
			glVertex2f(x, y + thick);
		}
		glEnd();

		glLineWidth(3.0f);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < samples; i++)
		{
			float t = (float)i / (float)(samples - 1);
			float x = x0 + (x1 - x0) * t;
			float y = y0 + (y1 - y0) * t;
			float envelope = sinf(t * 3.1415926f);
			y += fishOscSample(t, animTime) * amp * envelope;
			glVertex2f(x, y);
		}
		glEnd();

		glLineWidth(1.0f);
		glEnable(GL_TEXTURE_2D);
	}

	void drawOscFishBodyTransition(
		float x0,
		float y0,
		float x1,
		float y1,
		float alpha,
		float waveAmp,
		float animTime
	)
	{
		const int samples = 96;
		float amp = 42.0f + waveAmp * 0.18f;
		const float thickness = 13.0f;

		glDisable(GL_TEXTURE_2D);
		glColor4f(0.55f, 0.55f, 0.55f, alpha);

		glBegin(GL_TRIANGLE_STRIP);
		for (int i = 0; i < samples; i++)
		{
			float t = (float)i / (float)(samples - 1);
			float x = x0 + (x1 - x0) * t;
			float y = y0 + (y1 - y0) * t;
			float envelope = sinf(t * 3.1415926f);
			float xShift =
				sinf(animTime * 11.0f + t * 19.0f) * waveAmp * envelope;
			float yShift =
				sinf(animTime * 7.0f + t * 15.0f)
				* waveAmp * 0.25f * envelope;
			y += fishOscSample(t, animTime) * amp * envelope + yShift;
			float thick = thickness * envelope;

			glVertex2f(x + xShift, y - thick);
			glVertex2f(x + xShift, y + thick);
		}
		glEnd();

		glLineWidth(3.0f);
		glColor4f(1.0f, 1.0f, 1.0f, alpha);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < samples; i++)
		{
			float t = (float)i / (float)(samples - 1);
			float x = x0 + (x1 - x0) * t;
			float y = y0 + (y1 - y0) * t;
			float envelope = sinf(t * 3.1415926f);
			float xShift =
				sinf(animTime * 11.0f + t * 19.0f) * waveAmp * envelope;
			float yShift =
				sinf(animTime * 7.0f + t * 15.0f)
				* waveAmp * 0.25f * envelope;
			y += fishOscSample(t, animTime) * amp * envelope + yShift;
			glVertex2f(x + xShift, y);
		}
		glEnd();

		glLineWidth(1.0f);
		glEnable(GL_TEXTURE_2D);
	}

	LogoLayout getLogoLayout(float centerX, float centerY, float scale)
	{
		LogoLayout layout;
		layout.headW = 106.0f * scale;
		layout.headH = 94.0f * scale;
		layout.tailW = 70.0f * scale;
		layout.tailH = 94.0f * scale;

		float bodyLen = 130.0f * scale;
		float totalW = layout.headW + bodyLen + layout.tailW;

		layout.headX = centerX - totalW * 0.5f;
		layout.headY = centerY - layout.headH * 0.5f;
		layout.bodyStartX = layout.headX + layout.headW - 7.0f * scale;
		layout.bodyStartY = centerY;
		layout.bodyEndX = layout.bodyStartX + bodyLen;
		layout.bodyEndY = centerY;
		layout.tailX = layout.bodyEndX - 3.0f * scale;
		layout.tailY = centerY - layout.tailH * 0.5f;
		return layout;
	}

	void drawOscFishLogo(
		GLuint headTexture,
		GLuint tailTexture,
		float centerX,
		float centerY,
		float scale,
		float animTime
	)
	{
		LogoLayout layout = getLogoLayout(centerX, centerY, scale);
		drawTexturedQuad(
			headTexture,
			layout.headX,
			layout.headY,
			layout.headW,
			layout.headH
		);
		drawOscFishBody(
			layout.bodyStartX,
			layout.bodyStartY,
			layout.bodyEndX,
			layout.bodyEndY,
			animTime
		);
		drawTexturedQuad(
			tailTexture,
			layout.tailX,
			layout.tailY,
			layout.tailW,
			layout.tailH
		);
	}


	float logoScale()
	{
		int w = vscreen.width;
		int h = vscreen.height;
		return (w <= 400 || h <= 300) ? 0.6f : 2.0f;
	}
}




void loadFontLogo() {
	loadFont("./assets/arial.ttf", 64, &logoFont);
}

void drawLogoIntroEffect(
	GLuint headTexture,
	GLuint tailTexture,
	float animTime
)
{
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	begin2D();

	int w = vscreen.width;
	int h = vscreen.height;
	float scale = logoScale();

	drawOscFishLogo(
		headTexture,
		tailTexture,
		w * 0.5f,
		h * 0.5f,
		scale,
		animTime
	);

	end2D();
	
	printText(100, 300, "Very Important Pictures", 0xffffffff, logoFont);
}

void drawLogoTransitionEffect(
	GLuint headTexture,
	GLuint tailTexture,
	GLuint rotoTexture,
	float animTime,
	float duration,
	float rotoAnimTime
)
{
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawRotozoomerBase(rotoTexture, rotoAnimTime);
	begin2D();

	int w = vscreen.width;
	int h = vscreen.height;
	float scale = logoScale();
	float progress = (duration > 0.0f) ? animTime / duration : 0.0f;
	if (progress < 0.0f) progress = 0.0f;
	if (progress > 1.0f) progress = 1.0f;

	float eased = progress * progress * (3.0f - 2.0f * progress);
	float alpha = 1.0f - eased;
	float waveAmp = 6.0f + eased * 90.0f;
	float logoShiftX =
		sinf(animTime * 9.0f) * (4.0f + eased * 16.0f);
	LogoLayout layout =
		getLogoLayout(w * 0.5f + logoShiftX, h * 0.5f, scale);

	drawTexturedQuadWavy(
		headTexture,
		layout.headX,
		layout.headY,
		layout.headW,
		layout.headH,
		alpha,
		waveAmp,
		animTime
	);
	drawOscFishBodyTransition(
		layout.bodyStartX,
		layout.bodyStartY,
		layout.bodyEndX,
		layout.bodyEndY,
		alpha,
		waveAmp * 0.45f,
		animTime
	);
	drawTexturedQuadWavy(
		tailTexture,
		layout.tailX,
		layout.tailY,
		layout.tailW,
		layout.tailH,
		alpha,
		waveAmp,
		animTime
	);

	end2D();
}
