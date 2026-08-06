#ifndef FONT_H
#define FONT_H


#include <cstdint>

typedef struct
{
    void *data;
    unsigned int size;
} aFont;

#ifdef __cplusplus
extern "C" {
#endif

void loadFont(const char *fname, int h, aFont *fnt);
void closeFont(aFont *fnt);
void printText(int32_t x, int32_t y, const char *s, uint32_t color, aFont fnt);

#ifdef __cplusplus
}
#endif

#endif

