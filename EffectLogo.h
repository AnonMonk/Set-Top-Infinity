#ifndef EFFECT_LOGO_H
#define EFFECT_LOGO_H

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

void drawLogoIntroEffect(
	GLuint headTexture,
	GLuint tailTexture,
	float animTime
);

void drawLogoTransitionEffect(
	GLuint headTexture,
	GLuint tailTexture,
	GLuint rotoTexture,
	float animTime,
	float duration,
	float rotoAnimTime
);

void loadFontLogo();

#endif
