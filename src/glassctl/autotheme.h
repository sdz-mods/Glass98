#ifndef W98_AUTOTHEME_H
#define W98_AUTOTHEME_H
#include <windows.h>
#define WALL_SAMPLE 96
typedef struct
{
    unsigned long bg, fg, accent, low, mid, high;
    int alpha, light, busy;
    double contrast;
} WALL_PALETTE;
/* RGB values are 0xRRGGBB. mode: 0 auto, 1 dark, 2 light. */
int wall_palette(const unsigned long *pixels, int width, int height, int mode, int margins, WALL_PALETTE *out);
int wall_analyze(const char *path, int mode, int margins, WALL_PALETTE *out, char *error, int capacity);
double wall_luminance(unsigned long rgb);
unsigned long wall_blend(unsigned long foreground, unsigned long background, int percent);
#endif
