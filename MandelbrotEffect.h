#ifndef MANDELBROT_EFFECT_H
#define MANDELBROT_EFFECT_H

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

const int MANDELBROT_TEXTURE_COUNT = 20;

void drawMandelbrotEffect(
	const GLuint* textures,
	int textureCount,
	float animTime,
	float duration
);

#endif
