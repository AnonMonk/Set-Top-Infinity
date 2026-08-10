#ifndef EFFECT_JULIA_H
#define EFFECT_JULIA_H

/*
 * Echter CPU-Livepfad: In jedem Bild wird eine vollstaendige Julia berechnet.
 * SSE und Paletten-Lookups halten die Arbeit klein.
 */
void juliaLiveShutdown();

void drawJuliaEffect(float animTime, float duration);

#endif
