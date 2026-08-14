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

		/*
		 * Der Zustand wird aus der lokalen Szenenzeit rekonstruiert. Damit
		 * funktionieren Pause, Seek, Restart und Solo-Loop ohne eine alte
		 * globale G3-Timer-/Update-Schleife.
		 */
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

			/* Rollrichtung folgt der horizontalen Bewegung. */
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

			/* Senkrechte Wandlinien laufen perspektivisch in den Boden weiter. */
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
				/* Projektive Abstaende wie in der Boing-Ball-Vorlage. */
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
			/* Fast schwarze Rueckwand mit einem sehr leichten Gruenblau. */
			glColor3f(0.010f, 0.026f, 0.022f);
			glVertex3f(0.0f, 0.0f, -500.0f);
			glVertex3f(width, 0.0f, -500.0f);
			glColor3f(0.002f, 0.006f, 0.009f);
			glVertex3f(width, height, -500.0f);
			glVertex3f(0.0f, height, -500.0f);

			/* Der Boden wird nach vorn nur leicht dunkler. */
			glColor3f(0.004f, 0.014f, 0.012f);
			glVertex3f(0.0f, 0.0f, -400.0f);
			glVertex3f(width, 0.0f, -400.0f);
			glColor3f(0.012f, 0.038f, 0.027f);
			glVertex3f(width, floorY, -400.0f);
			glVertex3f(0.0f, floorY, -400.0f);
		glEnd();

		/* Breiter Glow plus scharfer hellgruener Neon-Kern. */
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
		float floorY)
	{
		float distance = y - radius - floorY;
		float heightFade = clamp01(1.0f - distance / (radius * 7.0f));
		float radiusX = radius * (0.72f + (1.0f - heightFade) * 0.48f);
		float radiusY = radius * 0.23f;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.0f, 0.0f, 0.0f, 0.12f + heightFade * 0.38f);
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

	void enableBallLight(float width, float height)
	{
		const GLfloat position[4] = {
			width * 0.28f, height * 0.92f, 620.0f, 1.0f
		};
		const GLfloat diffuse[4] = { 1.0f, 0.96f, 0.90f, 1.0f };
		const GLfloat ambient[4] = { 0.17f, 0.18f, 0.23f, 1.0f };
		const GLfloat specular[4] = { 0.95f, 0.95f, 1.0f, 1.0f };
		const GLfloat materialSpecular[4] = { 0.8f, 0.8f, 0.85f, 1.0f };

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_NORMALIZE);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		glLightfv(GL_LIGHT0, GL_POSITION, position);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
		glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 52.0f);
	}

	void disableBallLight()
	{
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_NORMALIZE);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHTING);
	}

	void drawBoingSphere(float radius)
	{
		const int slices = 16;
		const int stacks = 8;
		const float thetaStep = kTwoPi / (float)slices;
		const float phiStep = kPi / (float)stacks;

		glShadeModel(GL_SMOOTH);
		glBegin(GL_TRIANGLES);
		for (int stack = 0; stack < stacks; stack++)
		{
			float phi0 = (float)stack * phiStep;
			float phi1 = (float)(stack + 1) * phiStep;

			for (int slice = 0; slice < slices; slice++)
			{
				float theta0 = (float)slice * thetaStep;
				float theta1 = (float)(slice + 1) * thetaStep;
				float x00 = cosf(theta0) * sinf(phi0);
				float y00 = cosf(phi0);
				float z00 = sinf(theta0) * sinf(phi0);
				float x10 = cosf(theta1) * sinf(phi0);
				float y10 = y00;
				float z10 = sinf(theta1) * sinf(phi0);
				float x11 = cosf(theta1) * sinf(phi1);
				float y11 = cosf(phi1);
				float z11 = sinf(theta1) * sinf(phi1);
				float x01 = cosf(theta0) * sinf(phi1);
				float y01 = y11;
				float z01 = sinf(theta0) * sinf(phi1);

				if (((stack + slice) & 1) != 0)
					glColor3f(0.92f, 0.025f, 0.035f);
				else
					glColor3f(0.98f, 0.98f, 0.96f);

				glNormal3f(x00, y00, z00);
				glVertex3f(radius * x00, radius * y00, radius * z00);
				glNormal3f(x10, y10, z10);
				glVertex3f(radius * x10, radius * y10, radius * z10);
				glNormal3f(x11, y11, z11);
				glVertex3f(radius * x11, radius * y11, radius * z11);

				glNormal3f(x00, y00, z00);
				glVertex3f(radius * x00, radius * y00, radius * z00);
				glNormal3f(x11, y11, z11);
				glVertex3f(radius * x11, radius * y11, radius * z11);
				glNormal3f(x01, y01, z01);
				glVertex3f(radius * x01, radius * y01, radius * z01);
			}
		}
		glEnd();
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
	BallState state;

	calculateBallState(animTime, width, height, radius, floorY, &state);

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
	drawShadow(state.x, state.y, radius, floorY);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	enableBallLight(width, height);
	glPushMatrix();
		glTranslatef(state.x, state.y, 0.0f);
		glRotatef(-16.0f, 1.0f, 0.0f, 0.0f);
		/* Urspruengliche Drehachse, aber mit umgekehrtem Drehsinn. */
		glRotatef(-state.rotation, 0.15f, 1.0f, 0.0f);
		drawBoingSphere(radius);
	glPopMatrix();
	disableBallLight();
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);

	float fadeIn = 1.0f - smoothStep(animTime / 0.55f);
	float fadeOut = 0.0f;
	if (duration > 0.0f)
		fadeOut = smoothStep((animTime - (duration - 0.80f)) / 0.75f);
	drawFade(width, height, (fadeIn > fadeOut) ? fadeIn : fadeOut);

	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}
