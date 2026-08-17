#include "EffectEndTitles.h"

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

#include <cmath>

#include "glTTF.h"
#include "vipgfx.h"

namespace
{
	const float kScreenW = 1280.0f;
	const float kScreenH = 720.0f;
	const float kFontSize = 48.0f;
	const float kHeadingFontSize = 72.0f;
	const float kLineHeight = 56.0f;
	// Demo-Szenen CREDITS/GREETS sind je ~9s — Scroll so, dass der Block durchläuft
	const float kSceneDuration = 9.0f;
	const float kHorizontalWaveAmplitude = 70.0f;
	const float kHorizontalWaveSpeed = 2.25f;
	const float kHorizontalLinePhase = 0.72f;

	aFont titlesFont = {};
	aFont headingFont = {};

	const char* creditsLines[] =
	{
		"CREDITS",
		"",
		"ANON MONK PRESENTS",
		"",
		"SET-TOP INFINITY",
		"",
		"CODE:",
		"ANON MONK",
		"",
		"DESIGN:",
		"ANON MONK",
		"",
		"GRAPHICS:",
		"ANON MONK",
		"CHUDAK",
		"",
		"MUSIC:",
		"AUDIO MONSTERS",
		"",
		"ADDITIONAL CODE:",
		"KEY REAL",
		"",
		"C++ / OPENGL",
		"",
		"MADE FOR FUN.",
		"MADE FOR THE APPLE TV",
		"1ST GENERATION."
	};

	const char* greetsLines[] =
	{
		"GREETS",
		"",
		"",
		"TO RABENAUGE",
		"",
		"TO GNUMPF",
		"",
		"TO MANDELBRÖTCHEN",
		"",
		"TO EVERYONE STILL CODING DEMOS",
		"",
		"TO PIXEL PUSHERS",
		"",
		"TO TRACKER MUSICIANS",
		"",
		"TO OLD HARDWARE FANS",
		"",
		"DONT DRINK AND CODE"
	};

	const int creditsLineCount =
		(int)(sizeof(creditsLines) / sizeof(creditsLines[0]));
	const int greetsLineCount =
		(int)(sizeof(greetsLines) / sizeof(greetsLines[0]));

	int centeredX(const char* text, aFont font)
	{
		if (text == 0 || text[0] == '\0')
			return 0;

		uint32_t textWidth = 0;
		uint32_t textHeight = 0;
		getTextSize(font, text, &textWidth, &textHeight);
		return (int)((kScreenW - (float)textWidth) * 0.5f);
	}

	// Grün im vipgfx-Farbformat (RGBA-Helfer)
	unsigned int titlesGreen()
	{
		return RGBA((char)0, (char)255, (char)64, (char)255);
	}

	void drawScrollingLines(
		const char* const* lines,
		int lineCount,
		float animTime
	)
	{
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const float totalHeight = (float)lineCount * kLineHeight;
		// Start: erste Zeile unter dem Bild; Ende: letzter Text über dem oberen Rand
		const float travel = kScreenH + totalHeight + kFontSize;
		const float speed = travel / kSceneDuration;
		const float scroll = animTime * speed;
		const float y0 = kScreenH + kFontSize * 0.5f - scroll;

		const unsigned int color = titlesGreen();

		for (int i = 0; i < lineCount; i++)
		{
			const char* line = lines[i];
			if (line == 0 || line[0] == '\0')
				continue;

			const float y = y0 + (float)i * kLineHeight;
			if (y < -kLineHeight || y > kScreenH + kLineHeight)
				continue;

			/*
			 * Jede Zeile hat ihre eigene Phase. So schlaengelt sich der gesamte
			 * Endscroller zeilenweise von rechts nach links und wieder zurueck.
			 */
			const float phase =
				animTime * kHorizontalWaveSpeed
				+ (float)i * kHorizontalLinePhase;
			const int waveX =
				(int)(sinf(phase) * kHorizontalWaveAmplitude);

			if (i == 0)
			{
				printText(
					centeredX(line, headingFont) + waveX,
					(int)y,
					line,
					color,
					headingFont
				);
			}
			else
			{
				printText(
					centeredX(line, titlesFont) + waveX,
					(int)y,
					line,
					color,
					titlesFont
				);
			}
		}
	}
}

void loadFontEndTitles()
{
	loadFont("./assets/corbel.ttf", (int)kFontSize, &titlesFont);
	loadFont("./assets/Corbel Bold.ttf", (int)kHeadingFontSize, &headingFont);
}

void drawCreditsEffect(float animTime)
{
	drawScrollingLines(creditsLines, creditsLineCount, animTime);
}

void drawGreetsEffect(float animTime)
{
	drawScrollingLines(greetsLines, greetsLineCount, animTime);
}
