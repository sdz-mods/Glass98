/* Exercise the production profile logic using isolated INI files. */
#define WinMain ProfileWinMain
#include "../src/glassctl/glasspref.c"
#include <assert.h>
static void load(char *s)
{
    GetPrivateProfileStringA("Desktop", "Layout", "", s, 4096, ini);
    assert(valid(s));
}
static int enabled(char *s)
{
    char *a[FIELDS];
    int n = 0, i;
    layout_split(s, a);
    for (i = 0; i < PANELS; i++)
        n += atoi(a[5 + i * 8]);
    return n;
}
int main(int argc, char **argv)
{
    char high[4096], low[4096], now[4096], copy[4096], *a[FIELDS], *b[FIELDS], row[180], hidden[PANELS + 1];
    int sizes[PANELS], i, j, n;
    assert(argc == 2);
    lstrcpynA(ini, argv[1], sizeof(ini));
    DeleteFileA(ini);
    make_defaults();
    /* Start with eight enabled widgets, using the production layout schema. */
    strcpy(copy, defaults);
    layout_split(copy, a);
    now[0] = 0;
    for (i = 0; i < FIELDS; i++)
    {
        if (i)
            strcat(now, "|");
        if (i >= 5 && (i - 5) % 8 == 0 && (i - 5) / 8 < 9 && (i - 5) / 8 != 5)
            strcat(now, "1");
        else
            strcat(now, a[i]);
    }
    assert(WritePrivateProfileStringA("Desktop", "Layout", now, ini));
    for (i = 0; i < PANELS; i++)
        sizes[i] = 170;
    sizes[0] = 300;
    assert(layout_switch(1600, 1200, 1600, 1172, sizes));
    load(high);
    strcpy(copy, high);
    assert(enabled(copy) == 8);
    assert(layout_switch(800, 600, 800, 572, sizes));
    load(low);
    strcpy(copy, low);
    n = enabled(copy);
    assert(n > 0 && n < 8);
    layout_hidden(hidden);
    assert(strchr(hidden, '1'));
    /* Lower-resolution choice and shared appearance/refresh settings. */
    strcpy(copy, low);
    layout_split(copy, a);
    sprintf(row, "280,0,1,0,0,%s,%s,112233,FFFFFF,80FFFF,55,7,0", a[6], a[7]);
    assert(panel_save(0, row));
    load(low);
    assert(WritePrivateProfileStringA("Options", "drives", "C,D", ini));
    assert(layout_switch(1600, 1200, 1600, 1172, sizes));
    load(now);
    strcpy(copy, high);
    layout_split(copy, a);
    layout_split(now, b);
    for (i = 0; i < PANELS; i++)
        for (j = 0; j < 3; j++)
            assert(!strcmp(a[5 + i * 8 + j], b[5 + i * 8 + j]));
    assert(!strcmp(b[8], "112233") && !strcmp(b[12], "7"));
    assert(layout_switch(800, 600, 800, 572, sizes));
    load(now);
    assert(!strcmp(now, low));
    /* Same resolution with a smaller work area reflows safely, without a new key. */
    assert(layout_switch(800, 600, 800, 450, sizes));
    load(now);
    layout_split(now, a);
    for (i = 0; i < PANELS; i++)
        if (atoi(a[5 + i * 8]))
        {
            assert(atoi(a[6 + i * 8]) + 280 <= 798);
            assert(atoi(a[7 + i * 8]) + sizes[i] <= 447);
        }
    GetPrivateProfileStringA("Desktop", "Resolution", "", row, sizeof(row), ini);
    assert(!strcmp(row, "800x600"));
    /* A duplicate display notification must preserve a manual edit. */
    sprintf(row, "280,0,1,0,1,25,25,112233,FFFFFF,80FFFF,55,7,0");
    assert(panel_save(0, row));
    assert(layout_switch(800, 600, 800, 450, sizes));
    load(now);
    layout_split(now, a);
    assert(!strcmp(a[6], "25") && !strcmp(a[7], "25"));
    /* Explicit arrange reuses hidden candidates and clears its request. */
    assert(WritePrivateProfileStringA("Desktop", "Reflow", "1", ini));
    assert(layout_switch(800, 600, 800, 450, sizes));
    assert(GetPrivateProfileIntA("Desktop", "Reflow", 0, ini) == 0);
    strcpy(row, "0/280,0,1,0,1,1,1,000000,FFFFFF,80FFFF,55,2,0/999x999");
    assert(!layout_guard(row));
    GetPrivateProfileStringA("Options", "drives", "", row, sizeof(row), ini);
    assert(!strcmp(row, "C,D"));
    /* Rendering mode is a validated global option, independent of profiles/themes. */
    strcpy(row, "disableAlpha=31");
    assert(save_options(row));
    assert(GetPrivateProfileIntA("Options", "disableAlpha", 0, ini) == 1);
    strcpy(row, "disableAlpha=32");
    assert(!save_options(row));
    assert(GetPrivateProfileIntA("Options", "disableAlpha", 0, ini) == 1);
    assert(layout_switch(1600, 1200, 1600, 1172, sizes));
    assert(GetPrivateProfileIntA("Options", "disableAlpha", 0, ini) == 1);
    load(now);
    strcpy(row, "apply/1");
    assert(theme_action(row, ini, now));
    assert(GetPrivateProfileIntA("Options", "disableAlpha", 0, ini) == 1);
    strcpy(row, "disableAlpha=30");
    assert(save_options(row));
    assert(GetPrivateProfileIntA("Options", "disableAlpha", 1, ini) == 0);
    puts("PASS: opaque-mode validation, persistence, resolution and theme independence");
    puts("PASS: resolution visibility/positions, shared options, hidden overflow, work area and repeated notifications");
    return 0;
}
