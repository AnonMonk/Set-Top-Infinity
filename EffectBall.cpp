#include "EffectBall.h"

#include <cmath>

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

#include "vipgfx.h"
#include "EffectStarfield.h"

namespace
{
	const float kPi = 3.14159265358979323846f;
	const float kTwoPi = kPi * 2.0f;

	struct BallState
	{
		float x;
		float y;
		float vx;
		float vy;
		float rotation;
	};

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

	void calculateBallState(
		float animTime,
		float width,
		float height,
		float radius,
		float floorY,
		BallState* state)
	{
		state->x = width * 0.5f;
		state->y = height * 0.76f;
		state->vx = width * 0.20f;
		state->vy = 0.0f;
		state->rotation = 0.0f;

		// Rebuild from scene time so seek, restart, and solo loops stay deterministic.
		const float gravity = -height * 1.36f;
		const float restitution = 0.91f;
		const float minimumBounceSpeed = height * 0.67f;
		const float left = radius;
		const float right = width - radius;
		const float step = 1.0f / 240.0f;
		float remaining = animTime;

		if (remaining < 0.0f)
			remaining = 0.0f;

		while (remaining > 0.0f)
		{
			float dt = (remaining < step) ? remaining : step;
			state->vy += gravity * dt;
			state->x += state->vx * dt;
			state->y += state->vy * dt;

			if (state->y - radius < floorY)
			{
				state->y = floorY + radius;
				state->vy = -state->vy * restitution;
				if (state->vy < minimumBounceSpeed)
					state->vy = minimumBounceSpeed;
			}

			if (state->x < left)
			{
				state->x = left;
				state->vx = -state->vx;
			}
			else if (state->x > right)
			{
				state->x = right;
				state->vx = -state->vx;
			}

			state->rotation += state->vx / radius * dt * 180.0f / kPi;
			remaining -= dt;
		}

		state->rotation = fmodf(state->rotation, 360.0f);
	}

	void drawBackgroundGrid(
		float wallLeft,
		float wallRight,
		float wallTop,
		float floorLeft,
		float floorRight,
		float floorY,
		int columns,
		int wallRows,
		int floorRows)
	{
		glBegin(GL_LINES);
			for (int column = 0; column <= columns; column++)
			{
				float mix = (float)column / (float)columns;
				float x = wallLeft + (wallRight - wallLeft) * mix;
				glVertex3f(x, floorY, -350.0f);
				glVertex3f(x, wallTop, -350.0f);
			}
			for (int row = 0; row <= wallRows; row++)
			{
				float mix = (float)row / (float)wallRows;
				float y = floorY + (wallTop - floorY) * mix;
				glVertex3f(wallLeft, y, -350.0f);
				glVertex3f(wallRight, y, -350.0f);
			}

			for (int column = 0; column <= columns; column++)
			{
				float mix = (float)column / (float)columns;
				float horizonX = wallLeft + (wallRight - wallLeft) * mix;
				float floorX = floorLeft + (floorRight - floorLeft) * mix;
				glVertex3f(horizonX, floorY, -300.0f);
				glVertex3f(floorX, 0.0f, -300.0f);
			}
			for (int row = 0; row <= floorRows; row++)
			{
				float depth = (float)row / (float)floorRows;
				float perspective =
					depth / (2.30f - 1.30f * depth);
				float y = floorY * (1.0f - perspective);
				float left =
					wallLeft + (floorLeft - wallLeft) * perspective;
				float right =
					wallRight + (floorRight - wallRight) * perspective;
				glVertex3f(left, y, -300.0f);
				glVertex3f(right, y, -300.0f);
			}
		glEnd();
	}

	void drawBackground(float width, float height, float floorY)
	{
		const float wallLeft = width * 0.12f;
		const float wallRight = width * 0.88f;
		const float wallTop = height * 0.92f;
		const float floorLeft = width * 0.025f;
		const float floorRight = width * 0.975f;
		const int columns = 15;
		const int wallRows = 9;
		const int floorRows = 5;

		glShadeModel(GL_SMOOTH);
		glBegin(GL_QUADS);
			glColor3f(0.010f, 0.026f, 0.022f);
			glVertex3f(0.0f, 0.0f, -500.0f);
			glVertex3f(width, 0.0f, -500.0f);
			glColor3f(0.002f, 0.006f, 0.009f);
			glVertex3f(width, height, -500.0f);
			glVertex3f(0.0f, height, -500.0f);

			glColor3f(0.004f, 0.014f, 0.012f);
			glVertex3f(0.0f, 0.0f, -400.0f);
			glVertex3f(width, 0.0f, -400.0f);
			glColor3f(0.012f, 0.038f, 0.027f);
			glVertex3f(width, floorY, -400.0f);
			glVertex3f(0.0f, floorY, -400.0f);
		glEnd();

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glLineWidth(6.0f);
		glColor4f(0.05f, 1.0f, 0.28f, 0.18f);
		drawBackgroundGrid(
			wallLeft, wallRight, wallTop,
			floorLeft, floorRight, floorY,
			columns, wallRows, floorRows
		);
		glLineWidth(2.0f);
		glColor4f(0.30f, 1.0f, 0.46f, 0.96f);
		drawBackgroundGrid(
			wallLeft, wallRight, wallTop,
			floorLeft, floorRight, floorY,
			columns, wallRows, floorRows
		);
		glDisable(GL_BLEND);
		glLineWidth(1.0f);
	}

	void drawShadow(
		float x,
		float y,
		float radius,
		float floorY,
		float visibility)
	{
		if (visibility <= 0.0f)
			return;

		float distance = y - radius - floorY;
		float heightFade = clamp01(1.0f - distance / (radius * 7.0f));
		float radiusX = radius * (0.72f + (1.0f - heightFade) * 0.48f);
		float radiusY = radius * 0.23f;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(
			0.0f,
			0.0f,
			0.0f,
			(0.12f + heightFade * 0.38f) * visibility
		);
		glBegin(GL_TRIANGLE_FAN);
			glVertex3f(x, floorY + 2.0f, -200.0f);
			for (int segment = 0; segment <= 40; segment++)
			{
				float angle = kTwoPi * (float)segment / 40.0f;
				glVertex3f(
					x + cosf(angle) * radiusX,
					floorY + 2.0f + sinf(angle) * radiusY,
					-200.0f
				);
			}
		glEnd();
		glDisable(GL_BLEND);
	}

	void drawFade(float width, float height, float alpha)
	{
		if (alpha <= 0.0f)
			return;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.0f, 0.0f, 0.0f, alpha);
		glBegin(GL_QUADS);
			glVertex3f(0.0f, 0.0f, 1000.0f);
			glVertex3f(width, 0.0f, 1000.0f);
			glVertex3f(width, height, 1000.0f);
			glVertex3f(0.0f, height, 1000.0f);
		glEnd();
		glDisable(GL_BLEND);
	}
}

void drawBallEffect(float animTime, float duration)
{
	float width = (vscreen.width > 0) ? (float)vscreen.width : 1280.0f;
	float height = (vscreen.height > 0) ? (float)vscreen.height : 720.0f;
	float radius = ((width < height) ? width : height) * 0.11f;
	float floorY = height * 0.11f;
	const float morphDuration = 3.25f;
	float morphStart = duration - morphDuration;
	float transition = duration > morphDuration
		? clamp01((animTime - morphStart) / morphDuration)
		: 0.0f;
	float moveMorph = smoothTurn(transition);
	float surfaceMorph = smoothTurn((transition - 0.08f) / 0.92f);
	float backgroundMorph = smoothTurn((transition - 0.18f) / 0.82f);
	float shadowMorph = smoothTurn((transition - 0.05f) / 0.78f);
	BallState state;

	calculateBallState(animTime, width, height, radius, floorY, &state);

	float objectX = state.x + (width * 0.5f - state.x) * moveMorph;
	float objectY = state.y + (height * 0.5f - state.y) * moveMorph;
	float starfieldStartPulse = 1.0f + 0.050f * sinf(0.70f);
	float meteorRadius = height * 0.5f * 0.205f * starfieldStartPulse;
	float objectRadius =
		radius + (meteorRadius - radius) * surfaceMorph;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, width, 0.0, height, -2000.0, 2000.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	drawBackground(width, height, floorY);
	if (backgroundMorph > 0.0f)
		drawFade(width, height, backgroundMorph);
	drawShadow(
		objectX,
		objectY,
		objectRadius,
		floorY,
		1.0f - shadowMorph
	);

	glPushMatrix();
		glTranslatef(objectX, objectY, 0.0f);
		glRotatef(
			-16.0f * (1.0f - surfaceMorph),
			1.0f, 0.0f, 0.0f
		);
		glRotatef(
			-state.rotation * (1.0f - surfaceMorph),
			0.15f, 1.0f, 0.0f
		);
		drawStarfieldMeteorMorph(
			0.0f,
			objectRadius,
			surfaceMorph,
			0.0f
		);
	glPopMatrix();
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);

	float fadeIn = 1.0f - smoothStep(animTime / 0.55f);
	drawFade(width, height, fadeIn);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
