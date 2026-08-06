#ifndef TWISTER_EFFECT_H
#define TWISTER_EFFECT_H

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

void drawTwisterEffect(float animTime);

#endif
