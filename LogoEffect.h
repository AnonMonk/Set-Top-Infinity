#ifndef LOGO_EFFECT_H
#define LOGO_EFFECT_H

#ifdef __linux__
#include <GL/gl.h>
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
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
