/* Bounded, deterministic palette selection; no desktop capture or network. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "autotheme.h"
static double linear[256];
static int initialized;
static int channel(unsigned long c, int shift)
{
    return (int)((c >> shift) & 255);
}
unsigned long wall_blend(unsigned long a, unsigned long b, int percent)
{
    int s;
    unsigned long r = 0;
    for (s = 0; s <= 16; s += 8)
        r |= (unsigned long)((channel(a, s) * percent + channel(b, s) * (100 - percent) + 50) / 100) << s;
    return r;
}
double wall_luminance(unsigned long c)
{
    int j;
    double v;
    if (!initialized)
    {
        for (j = 0; j < 256; j++)
        {
            v = j / 255.0;
            linear[j] = v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
        }
        initialized = 1;
    }
    return .2126 * linear[channel(c, 16)] + .7152 * linear[channel(c, 8)] + .0722 * linear[channel(c, 0)];
}
static double ratio(double a, double b)
{
    return a > b ? (a + .05) / (b + .05) : (b + .05) / (a + .05);
}
static double color_contrast(unsigned long c, double lo, double hi, int light)
{
    double lum = wall_luminance(c);
    if (light)
        return lum < lo ? ratio(lum, lo) : 1;
    return lum > hi ? ratio(lum, hi) : 1;
}
static unsigned long readable(unsigned long c, double lo, double hi, int light, double target)
{
    int n;
    unsigned long end = light ? 0x000000 : 0xFFFFFF, adjusted;
    for (n = 0; n <= 100; n += 2)
    {
        adjusted = wall_blend(end, c, n);
        if (color_contrast(adjusted, lo, hi, light) >= target)
            return adjusted;
    }
    return end;
}
static void limits(const unsigned long *pixels, int count, unsigned long bg, int alpha, int margins, double *lo,
                   double *hi)
{
    int i;
    double l;
    *lo = 1;
    *hi = 0;
    for (i = 0; i < count + margins; i++)
    {
        l = wall_luminance(wall_blend(bg, i < count ? pixels[i] : 0x0B1820, alpha));
        if (l < *lo)
            *lo = l;
        if (l > *hi)
            *hi = l;
    }
}
static void candidate(const unsigned long *pixels, int count, unsigned long tint, unsigned long seed, int light,
                      int busy, int margins, WALL_PALETTE *p)
{
    double lo, hi;
    int alpha, k, n;
    unsigned long c;
    p->light = light;
    p->busy = busy != 0;
    p->bg = wall_blend(tint, light ? 0xFFFFFF : 0x000000, light ? 8 : 13);
    p->fg = light ? 0x101820 : 0xF6F8FA;
    /* More image detail merits extra coverage even if the image is uniformly dark. */
    for (alpha = busy ? busy : 40; alpha < 96; alpha++)
    {
        limits(pixels, count, p->bg, alpha, margins, &lo, &hi);
        if (color_contrast(p->fg, lo, hi, light) >= 7.0)
            break;
    }
    p->alpha = alpha;
    limits(pixels, count, p->bg, alpha, margins, &lo, &hi);
    p->contrast = color_contrast(p->fg, lo, hi, light);
    p->accent = readable(seed, lo, hi, light, 4.6);
    p->low = readable(wall_blend(seed, 0x35B877, 12), lo, hi, light, 3.2);
    p->mid = readable(wall_blend(seed, 0xE8AE37, 12), lo, hi, light, 3.2);
    p->high = readable(wall_blend(seed, 0xE65360, 12), lo, hi, light, 3.2);
    /* Check interpolated meter colors too, not only the three end points. */
    for (n = 0; n < 16; n++)
    {
        for (k = 0; k <= 100; k++)
        {
            c = k <= 50 ? wall_blend(p->mid, p->low, k * 2) : wall_blend(p->high, p->mid, (k - 50) * 2);
            if (color_contrast(c, lo, hi, light) < 3.0)
                break;
        }
        if (k > 100)
            break;
        p->low = wall_blend(p->fg, p->low, 5);
        p->mid = wall_blend(p->fg, p->mid, 5);
        p->high = wall_blend(p->fg, p->high, 5);
    }
}
typedef struct
{
    unsigned long count, r, g, b;
} BIN;
int wall_palette(const unsigned long *pixels, int width, int height, int mode, int margins, WALL_PALETTE *out)
{
    BIN *bins;
    unsigned long r = 0, g = 0, b = 0, c, seed, tint;
    int count = width * height, i, j, red, green, blue, max, min, index, best = -1, busy;
    double score, bestScore = 0, mean = 0, detail = 0;
    WALL_PALETTE dark, light;
    if (!pixels || width < 1 || height < 1 || width > WALL_SAMPLE || height > WALL_SAMPLE || mode < 0 || mode > 2)
        return 0;
    bins = (BIN*)calloc(4096, sizeof(BIN));
    if (!bins)
        return 0;
    for (i = 0; i < count; i++)
    {
        c = pixels[i];
        red = channel(c, 16);
        green = channel(c, 8);
        blue = channel(c, 0);
        r += red;
        g += green;
        b += blue;
        mean += wall_luminance(c);
        index = ((red >> 4) << 8) | ((green >> 4) << 4) | (blue >> 4);
        bins[index].count++;
        bins[index].r += red;
        bins[index].g += green;
        bins[index].b += blue;
        if (i % width)
            detail += fabs(wall_luminance(c) - wall_luminance(pixels[i - 1]));
        if (i >= width)
            detail += fabs(wall_luminance(c) - wall_luminance(pixels[i - width]));
    }
    for (j = 0; j < 4096; j++)
        if (bins[j].count)
        {
            red = bins[j].r / bins[j].count;
            green = bins[j].g / bins[j].count;
            blue = bins[j].b / bins[j].count;
            max = red > green ? red : green;
            if (blue > max)
                max = blue;
            min = red < green ? red : green;
            if (blue < min)
                min = blue;
            if (max - min < 28 || max < 45)
                continue;
            score = bins[j].count * (.25 + (max - min) / 255.0);
            if (score > bestScore)
            {
                bestScore = score;
                best = j;
            }
        }
    seed = best < 0 ? 0x699BB8 : ((bins[best].r / bins[best].count) << 16) | ((bins[best].g / bins[best].count) << 8) |
           (bins[best].b / bins[best].count);
    tint = wall_blend(seed, ((r / count) << 16) | ((g / count) << 8) | (b / count), 35);
    free(bins);
    detail /= count;
    busy = detail > .16 ? 65 + (int)((detail - .16) * 35) : 0;
    if (busy > 85)
        busy = 85;
    mean /= count;
    candidate(pixels, count, tint, seed, 0, busy, margins, &dark);
    candidate(pixels, count, tint, seed, 1, busy, margins, &light);
    if (mode == 1)
        *out = dark;
    else if (mode == 2)
        *out = light;
    else if (mean < .36)
        *out = dark;
    else if (mean > .6)
        *out = light;
    else *out = light.alpha + 8 < dark.alpha ? light : dark;
    return 1;
}
