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

#include "glTTF.h"
#include "vipgfx.h"

#include <cstring>

namespace
{
	const float kScreenW = 1280.0f;
	const float kScreenH = 720.0f;
	const float kFontSize = 48.0f;
	const float kLineHeight = 56.0f;
	// Demo-Szenen CREDITS/GREETS sind je ~9s — Scroll so, dass der Block durchläuft
	const float kSceneDuration = 9.0f;
	// Zentrier-Faktor (ähnlich Logo-Text, bei dir ~0.40)
	const float kCharWidthFactor = 0.40f;

	aFont titlesFont = {};

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
		"ADDIS BEATS",
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
		"TO JULIA",
		"",
		"TO EVERYONE STILL CODING DEMOS",
		"",
		"TO PIXEL PUSHERS",
		"",
		"TO TRACKER MUSICIANS",
		"",
		"TO OLD HARDWARE FANS"
	};

	const int creditsLineCount =
		(int)(sizeof(creditsLines) / sizeof(creditsLines[0]));
	const int greetsLineCount =
		(int)(sizeof(greetsLines) / sizeof(greetsLines[0]));

	int centeredX(const char* text)
	{
		if (text == 0 || text[0] == '\0')
			return 0;

		const float estimatedW =
			(float)strlen(text) * kFontSize * kCharWidthFactor;
		return (int)((kScreenW - estimatedW) * 0.5f);
	}

	// Grün im vipgfx-Farbformat (RGBA-Helfer)
	unsigned int titlesGreen()
	{
		return RGBA(0, 255, 64, 255);
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

			printText(centeredX(line), (int)y, line, color, titlesFont);
		}
	}
}

void loadFontEndTitles()
{
	loadFont("./assets/corbel.ttf", (int)kFontSize, &titlesFont);
}

void drawCreditsEffect(float animTime)
{
	drawScrollingLines(creditsLines, creditsLineCount, animTime);
}

void drawGreetsEffect(float animTime)
{
	drawScrollingLines(greetsLines, greetsLineCount, animTime);
}
