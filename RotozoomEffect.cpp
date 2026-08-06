#include "RotozoomEffect.h"

#include <cmath>
#include "vipgfx.h"
namespace
{
	void rotoTextureCoord(float x, float y, float animTime)
	{
		float effectTime = animTime * 1.25f;
		float angle = effectTime * 0.60f;
		float zoom = 0.78f + 0.15f * sinf(effectTime * 0.90f);

		float cx = 0.5f + 0.14f * sinf(effectTime * 0.36f);
		float cy = 0.5f + 0.14f * cosf(effectTime * 0.48f);

		float ca = cosf(angle);
		float sa = sinf(angle);
		float u = cx + (x * ca - y * sa) * zoom;
		float v = cy + (x * sa + y * ca) * zoom;

		glTexCoord2f(u, v);
	}

	void drawRotozoomer(GLuint texture, float animTime, bool clearScreen)
	{
		if (clearScreen)
		{
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glDisable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, texture);
		glColor3f(1, 1, 1);

		glBegin(GL_QUADS);
			rotoTextureCoord(-0.5f,  0.5f, animTime); glVertex2f(-1, -1);
			rotoTextureCoord( 0.5f,  0.5f, animTime); glVertex2f( 1, -1);
			rotoTextureCoord( 0.5f, -0.5f, animTime); glVertex2f( 1,  1);
			rotoTextureCoord(-0.5f, -0.5f, animTime); glVertex2f(-1,  1);
		glEnd();
	}
}

void drawRotozoomerEffect(GLuint texture, float animTime)
{
	drawRotozoomer(texture, animTime, true);
}

void drawRotozoomerBase(GLuint texture, float animTime)
{
	drawRotozoomer(texture, animTime, false);
}
