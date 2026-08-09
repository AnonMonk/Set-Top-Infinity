#ifndef EFFECT_JULIA_H
#define EFFECT_JULIA_H

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

/*
 * Julia-Set Stufen-Precalc (kein PNG) — SPAR-Preset (ATV / 10%-Test):
 * 60 Levels @ 512x288 — schaerfer, etwas teurer Precalc.
 * Zoom in der z-Ebene, c fest. Gestueckelt bis inkl. Starfield.
 */
const int JULIA_TEXTURE_COUNT = 60;
const int JULIA_WIDTH = 512;
const int JULIA_HEIGHT = 288;

/* Zoom-Tiefe etwas kuerzer = weniger Iterationen in den tiefen Stufen */
const double JULIA_ZOOM_DEPTH = 16.0;

/* Profil C — ms Precalc pro Frame */
const double JULIA_BUDGET_LOGO_MS = 10.0;
const double JULIA_BUDGET_WAVE_MS = 9.0;
const double JULIA_BUDGET_ROTO_MS = 9.0;
const double JULIA_BUDGET_TUNNEL_MS = 8.0;
const double JULIA_BUDGET_TWISTER_MS = 7.0;
const double JULIA_BUDGET_STARFIELD_MS = 7.0; /* +20 s Puffer vor Julia */
const double JULIA_BUDGET_SOLO_MS = 12.0;

void juliaPrecalcInit(GLuint* outTextures);
void juliaPrecalcTick(double maxMs);
void juliaPrecalcFinishBlocking();
bool juliaPrecalcIsReady();
int juliaPrecalcLevelsDone();
void juliaPrecalcShutdown();

void drawJuliaEffect(
	const GLuint* textures,
	int textureCount,
	float animTime,
	float duration
);

#endif
