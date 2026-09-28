/* Native palette/decoder/profile integration tests; also runs on Win98. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/glassctl/autotheme.h"
#include "../src/glassctl/themes.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static double contrast(double a, double b)
{
    return a > b ? (a + .05) / (b + .05) : (b + .05) / (a + .05);
}
static int verify(unsigned long *pixels, WALL_PALETTE *p)
{
    int i, k;
    double lo = 1, hi = 0, v, c;
    unsigned long meter;
    CHECK(p->alpha >= 40 && p->alpha <= 96);
    for (i = 0; i < WALL_SAMPLE * WALL_SAMPLE + 1; i++)
    {
        v = wall_luminance(wall_blend(p->bg, i < WALL_SAMPLE * WALL_SAMPLE ? pixels[i] : 0x0B1820, p->alpha));
        if (v < lo)
            lo = v;
        if (v > hi)
            hi = v;
        CHECK(contrast(v, wall_luminance(p->fg)) >= 6.9);
        CHECK(contrast(v, wall_luminance(p->accent)) >= 4.5);
    }
    for (k = 0; k <= 100; k++)
    {
        meter = k <= 50 ? wall_blend(p->mid, p->low, k * 2) : wall_blend(p->high, p->mid, (k - 50) * 2);
        c = wall_luminance(meter);
        CHECK(contrast(c, lo) >= 3.0 && contrast(c, hi) >= 3.0);
    }
    return 0;
}
static int bmp(const char *path)
{
    FILE *f;
    BITMAPFILEHEADER header;
    BITMAPINFOHEADER info;
    unsigned char row[96 * 3];
    int i, j;
    memset(&header, 0, sizeof(header));
    memset(&info, 0, sizeof(info));
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + sizeof(row) * 96;
    info.biSize = sizeof(info);
    info.biWidth = 96;
    info.biHeight = 96;
    info.biPlanes = 1;
    info.biBitCount = 24;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite(&header, 1, sizeof(header), f);
    fwrite(&info, 1, sizeof(info), f);
    for (i = 0; i < 96; i++)
    {
        for (j = 0; j < 96; j++)
        {
            row[j * 3] = 60 + i;
            row[j * 3 + 1] = 80 + j;
            row[j * 3 + 2] = 30;
        }
        fwrite(row, 1, sizeof(row), f);
    }
    return fclose(f) == 0;
}
static void put32(unsigned char *p, unsigned long v)
{
    int i;
    for (i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (i * 8));
}
static unsigned long test_color(int x, int y)
{
    return ((unsigned long)(x * 2) << 16) | ((unsigned long)(y * 2) << 8) | ((x + y) & 255);
}
/* Odd dimensions exercise padding/resampling; the gap exercises bfOffBits. */
static int bmp_variant(const char *path, int header, int depth, int top, int damage)
{
    unsigned char prefix[162], row[388];
    unsigned long stride = (97 * (depth / 8) + 3) & ~3UL, offset = 14 + header + 17, c;
    FILE *f;
    int x, y;
    memset(prefix, 0, sizeof(prefix));
    prefix[0] = 'B';
    prefix[1] = 'M';
    put32(prefix + 2, offset + stride * 95);
    put32(prefix + 10, damage == 2 ? 20 : offset);
    put32(prefix + 14, header);
    put32(prefix + 18, 97);
    put32(prefix + 22, top ? (unsigned long) -95 : 95);
    prefix[26] = 1;
    prefix[28] = depth;
    if (header >= 108)
        put32(prefix + 70, 0x73524742UL);
    if (header == 124)
        put32(prefix + 122, 2);
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite(prefix, 1, offset, f);
    for (y = 0; y < 95 - (damage == 1); y++)
    {
        memset(row, 0xA5, sizeof(row));
        for (x = 0; x < 97; x++)
        {
            c = test_color(x, top ? y : 94 - y);
            row[x * (depth / 8)] = (unsigned char)c;
            row[x * (depth / 8) + 1] = (unsigned char)(c >> 8);
            row[x * (depth / 8) + 2] = (unsigned char)(c >> 16);
        }
        fwrite(row, 1, stride, f);
    }
    return fclose(f) == 0;
}
static int bmp_regressions(const char *path)
{
    static unsigned long samples[96 * 96];
    int headers[3] = {40, 108, 124}, v, depth, top, x, y;
    WALL_PALETTE expected, actual;
    char error[200];
    for (y = 0; y < 96; y++)
        for (x = 0; x < 96; x++)
            samples[y * 96 + x] = test_color(((2 * x + 1) * 97) / 192, ((2 * y + 1) * 95) / 192);
    CHECK(wall_palette(samples, 96, 96, 0, 1, &expected));
    for (v = 0; v < 3; v++)
        for (depth = 24; depth <= 32; depth += 8)
            for (top = 0; top < 2; top++)
            {
                CHECK(bmp_variant(path, headers[v], depth, top, 0));
                CHECK(wall_analyze(path, 0, 1, &actual, error, sizeof(error)));
                CHECK(actual.bg == expected.bg && actual.fg == expected.fg && actual.accent == expected.accent &&
                      actual.alpha == expected.alpha && actual.low == expected.low && actual.mid == expected.mid &&
                      actual.high == expected.high && actual.busy == expected.busy);
            }
    for (v = 1; v <= 2; v++)
    {
        CHECK(bmp_variant(path, 124, 24, 0, v));
        CHECK(!wall_analyze(path, 0, 1, &actual, error, sizeof(error)));
        CHECK(strstr(error, "BMP") != NULL);
    }
    return 0;
}
int main(int argc, char **argv)
{
    static unsigned long pixels[96 * 96];
    WALL_PALETTE p, again;
    int mode, pattern, i;
    unsigned long random = 33;
    char image[MAX_PATH], ini[MAX_PATH], relative[MAX_PATH], error[200], layout[4096], original[4096], value[4096],
         command[80];
    DWORD start = GetTickCount();
    FILE *f;
    if (argc > 1)
    {
        CHECK(wall_analyze(argv[1], argc > 2 ? atoi(argv[2]) : 0, 1, &p, error, sizeof(error)) || (printf("%s\n", error), 0));
        printf("%s: bg=%06lX fg=%06lX accent=%06lX opacity=%d light=%d text=%.2f busy=%d elapsed=%lu ms\n", argv[1], p.bg, p.fg,
               p.accent, p.alpha, p.light, p.contrast, p.busy, GetTickCount() - start);
        return 0;
    }
    for (pattern = 0; pattern < 8; pattern++)
    {
        for (i = 0; i < 96 * 96; i++)
        {
            random = random * 1664525UL + 1013904223UL;
            pixels[i] = pattern == 0 ? 0 : pattern == 1 ? 0xFFFFFF : pattern == 2 ? 0x167442 : pattern == 3 ? 0xDE6430 : pattern ==
                        4 ? 0x888888 : pattern == 5 ? (i % 2 ? 0xFFFFFF : 0) : pattern == 6 ? (i == 9215 ? 0xFFFFFF : 0x102040) : random &
                        0xFFFFFF;
        }
        for (mode = 0; mode < 3; mode++)
        {
            CHECK(wall_palette(pixels, 96, 96, mode, 1, &p));
            CHECK(!verify(pixels, &p));
            CHECK(wall_palette(pixels, 96, 96, mode, 1, &again));
            CHECK(p.bg == again.bg && p.accent == again.accent && p.alpha == again.alpha);
            if (mode)
                CHECK(p.light == (mode == 2));
            if (!mode && pattern == 0)
                CHECK(!p.light);
            if (!mode && pattern == 1)
                CHECK(p.light);
            if (pattern == 5)
            {
                CHECK(p.busy);
                CHECK(p.alpha >= 80);
            }
        }
    }
    CHECK(!wall_palette(pixels, 0, 96, 0, 1, &p));
    CHECK(!wall_analyze("not-present-wallpaper.bmp", 0, 1, &p, error, sizeof(error)));
    CHECK(*error);
    CHECK(GetTempFileNameA(".", "wai", 0, relative));
    CHECK(GetFullPathNameA(relative, sizeof(image), image, NULL));
    CHECK(!bmp_regressions(image));
    CHECK(bmp(image));
    CHECK(wall_analyze(image, 0, 1, &p, error, sizeof(error)));
    CHECK(GetTempFileNameA(".", "wap", 0, relative));
    CHECK(GetFullPathNameA(relative, sizeof(ini), ini, NULL));
    strcpy(layout, "3|280|1|1|3");
    for (i = 0; i < 22; i++)
        strcat(layout, "|1|10|20|010203|F0F1F2|AABBCC|55|9");
    strcpy(original, layout);
    WritePrivateProfileStringA("Desktop", "Wallpaper", image, ini);
    WritePrivateProfileStringA("Options", "notes", "Keep me", ini);
    WritePrivateProfileStringA("Options", "meterColors", "0", ini);
    CHECK(theme_action("auto/0/1", ini, layout));
    CHECK(strcmp(layout, original));
    CHECK(!strncmp(layout, "3|280|1|1|3|1|10|20|", strlen("3|280|1|1|3|1|10|20|")));
    CHECK(GetPrivateProfileIntA("AutoTheme", "Follow", 0, ini) == 1);
    CHECK(GetPrivateProfileIntA("Options", "meterColors", 1, ini) == 0);
    CHECK(theme_action("apply/1000", ini, layout));
    CHECK(!strcmp(layout, original));
    CHECK(GetPrivateProfileIntA("AutoTheme", "Follow", 1, ini) == 0);
    CHECK(theme_action("apply/1001", ini, layout));
    CHECK(theme_action("auto/2/1", ini, layout));
    CHECK(theme_action("new/57616c6c70617065722074657374", ini, layout));
    strcpy(original, layout);
    CHECK(theme_action("refresh", ini, layout));
    CHECK(!strcmp(layout, original));
    WritePrivateProfileStringA("Desktop", "Wallpaper", "Z:\\MISSING.BMP", ini);
    CHECK(!theme_action("refresh", ini, layout));
    CHECK(!strcmp(layout, original));
    GetPrivateProfileStringA("Options", "notes", "", value, sizeof(value), ini);
    CHECK(!strcmp(value, "Keep me"));
    CHECK(theme_action("unfollow", ini, layout));
    CHECK(theme_action("refresh", ini, layout));
    CHECK(!theme_action("auto/3/1", ini, layout));
    CHECK(!theme_action("auto/0/2", ini, layout));
    CHECK(theme_action("apply/0", ini, layout));
    CHECK(theme_action("apply/100", ini, layout));
    CHECK(!strcmp(layout, original));
    f = fopen(image, "wb");
    CHECK(f != NULL);
    fputs("Not an image", f);
    fclose(f);
    CHECK(!wall_analyze(image, 0, 1, &p, error, sizeof(error)));
    CHECK(bmp(image));
    f = fopen(image, "r+b");
    CHECK(f != NULL);
    random = 10000;
    fseek(f, 18, SEEK_SET);
    fwrite(&random, 4, 1, f);
    fclose(f);
    CHECK(!wall_analyze(image, 0, 1, &p, error, sizeof(error)));
    CHECK(strstr(error, "8192") != NULL);
    WritePrivateProfileStringA(NULL, NULL, NULL, ini);
    DeleteFileA(ini);
    DeleteFileA(image);
    printf("PASS: contrast, meter gradients, dark/light selection, difficult images, decoder, undo, save/restore, following, failure preservation (%lu ms)\n",
           GetTickCount() - start);
    return 0;
}
