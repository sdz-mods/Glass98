/* Themes store appearance only, independently of layout and widget contents.
   Caller owns the existing settings mutex and validates the current layout. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "themes.h"
#include "autotheme.h"
#include "../widgets.h"
#define COUNT GLASS98_PANELS
#define FIELDS (5+COUNT*8)
#define STYLE_FIELDS (5+COUNT*4)
#define CUSTOM 16
typedef struct
{
    const char *name, *bg, *fg, *accent, *low, *mid, *high;
    int alpha, dynamic;
} PRESET;
static const PRESET presets[] =
{
    {"Classic glass", "000000", "FFFFFF", "80FFFF", "62D98B", "FFD166", "FF6262", 55, 0},
    {"Graphite", "15191F", "F0F3F7", "AAB8CC", "68DDB0", "FFD166", "FF6B78", 85, 1},
    {"Midnight blue", "09192D", "EAF2FF", "79BCFF", "59D6CB", "AAACFF", "FF799A", 78, 1},
    {"Amber terminal", "120D05", "FFE8B0", "FFC45C", "91CE72", "FFC45C", "FF6452", 88, 0},
    {"Paper", "F4F1E8", "202B35", "245F95", "237847", "A06B08", "C33535", 96, 1}
};
#define PRESETS (sizeof(presets)/sizeof(presets[0]))
static int split(char *s, char **a, int count)
{
    int i;
    char *p = s;
    for (i = 0; i < count; i++)
    {
        a[i] = p;
        p = strchr(p, '|');
        if (i < count - 1 && !p)
            return 0;
        if (p)
            *p++ = 0;
    }
    return p == NULL;
}
static int color(const char *s)
{
    int i;
    if (strlen(s) != 6)
        return 0;
    for (i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)s[i]))
            return 0;
    return 1;
}
static int style_valid(const char *s)
{
    char copy[2048], *a[STYLE_FIELDS];
    int i, j;
    if (strlen(s) >= sizeof(copy))
        return 0;
    strcpy(copy, s);
    if (!split(copy, a, STYLE_FIELDS) || strcmp(a[0], "1") || (strcmp(a[1], "0") && strcmp(a[1], "1")))
        return 0;
    for (i = 2; i < STYLE_FIELDS; i++)
    {
        if (i < 5 || (i - 5) % 4 != 3)
        {
            if (!color(a[i]))
                return 0;
        }
        else
        {
            if (!*a[i] || strlen(a[i]) > 3)
                return 0;
            for (j = 0; a[i][j]; j++)
                if (!isdigit((unsigned char)a[i][j]))
                    return 0;
            if (atoi(a[i]) > 100)
                return 0;
        }
    }
    return 1;
}
static void option(const char *ini, const char *name, const char *fallback, char *out)
{
    GetPrivateProfileStringA("Options", name, fallback, out, 16, ini);
}
static int capture(const char *ini, const char *layout, char *out)
{
    char copy[4096], *a[FIELDS], low[16], mid[16], high[16], row[80];
    int i;
    strcpy(copy, layout);
    if (!split(copy, a, FIELDS))
        return 0;
    option(ini, "meterLow", "62D98B", low);
    option(ini, "meterMid", "FFD166", mid);
    option(ini, "meterHigh", "FF6262", high);
    if (!color(low))
        strcpy(low, "62D98B");
    if (!color(mid))
        strcpy(mid, "FFD166");
    if (!color(high))
        strcpy(high, "FF6262");
    sprintf(out, "1|%d|%s|%s|%s", GetPrivateProfileIntA("Options", "meterColors", 0, ini) != 0, low, mid, high);
    for (i = 0; i < COUNT; i++)
    {
        sprintf(row, "|%s|%s|%s|%d", a[8 + i * 8], a[9 + i * 8], a[10 + i * 8], atoi(a[11 + i * 8]));
        strcat(out, row);
    }
    return style_valid(out);
}
static void section(int id, char *out)
{
    if (id == 1000)
        strcpy(out, "ThemePrevious");
    else if (id == 1001)
        strcpy(out, "ThemeWallpaper");
    else
        sprintf(out, "Theme%d", id - 100);
}
static int load(const char *ini, int id, char *name, char *data)
{
    int i;
    char sec[32], row[80];
    const PRESET *p;
    if (id >= 0 && id < (int)PRESETS)
    {
        p = &presets[id];
        strcpy(name, p->name);
        sprintf(data, "1|%d|%s|%s|%s", p->dynamic, p->low, p->mid, p->high);
        for (i = 0; i < COUNT; i++)
        {
            sprintf(row, "|%s|%s|%s|%d", p->bg, p->fg, p->accent, p->alpha);
            strcat(data, row);
        }
        return 1;
    }
    if (id != 1000 && id != 1001 && (id < 100 || id >= 100 + CUSTOM))
        return 0;
    section(id, sec);
    GetPrivateProfileStringA(sec, "Name", "", name, 80, ini);
    GetPrivateProfileStringA(sec, "Data", "", data, 2048, ini);
    if (*name)
    {
        int fields = 1, index;
        char *scan, copy[2048], *values[93], row[80];
        for (scan = data; *scan; scan++)
            if (*scan == '|')
                fields++;
        if (fields == 93)
        {
            strcpy(copy, data);
            if (split(copy, values, 93) && color(values[5]) && color(values[6]) && color(values[7]) &&
                    *values[8] && strlen(values[8]) <= 3)
            {
                sprintf(row, "|%s|%s|%s|%s", values[5], values[6], values[7], values[8]);
                if (strlen(data) + (COUNT - 22) * strlen(row) >= 2048)
                    return 0;
                for (index = 22; index < COUNT; index++)
                    strcat(data, row);
            }
        }
    }
    return *name && style_valid(data);
}
static int put(const char *ini, int id, const char *name, const char *data)
{
    char sec[32];
    section(id, sec);
    return WritePrivateProfileStringA(sec, "Data", data, ini) && WritePrivateProfileStringA(sec, "Name", name, ini);
}
static int apply(const char *ini, char *layout, const char *data)
{
    char style[2048], current[4096], result[4096], *a[STYLE_FIELDS], *b[FIELDS], row[160];
    int i;
    strcpy(style, data);
    strcpy(current, layout);
    if (!style_valid(data) || !split(style, a, STYLE_FIELDS) || !split(current, b, FIELDS))
        return 0;
    sprintf(result, "%s|%s|%s|%s|%s", b[0], b[1], b[2], b[3], b[4]);
    for (i = 0; i < COUNT; i++)
    {
        sprintf(row, "|%s|%s|%s|%s|%s|%s|%s|%s", b[5 + i * 8], b[6 + i * 8], b[7 + i * 8], a[5 + i * 4], a[6 + i * 4],
                a[7 + i * 4], a[8 + i * 4], b[12 + i * 8]);
        strcat(result, row);
    }
    if (!WritePrivateProfileStringA("Options", "meterColors", a[1], ini) ||
            !WritePrivateProfileStringA("Options", "meterLow", a[2], ini) ||
            !WritePrivateProfileStringA("Options", "meterMid", a[3], ini) ||
            !WritePrivateProfileStringA("Options", "meterHigh", a[4], ini) ||
            !WritePrivateProfileStringA("Desktop", "Layout", result, ini))
        return 0;
    strcpy(layout, result);
    return 1;
}
static int decode_name(const char *hex, char *name)
{
    char bytes[33];
    WCHAR wide[33];
    unsigned int v;
    int i, n = strlen(hex);
    if (!n || n > 64 || n % 2)
        return 0;
    for (i = 0; i < n; i += 2)
    {
        if (!isxdigit(hex[i]) || !isxdigit(hex[i + 1]) || sscanf(hex + i, "%2x", &v) != 1 || v < 32)
            return 0;
        bytes[i / 2] = (char)v;
    }
    bytes[n / 2] = 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, bytes, -1, wide, 33) ||
            !WideCharToMultiByte(CP_ACP, 0, wide, -1, name, 80, NULL, NULL))
        return 0;
    n = strlen(name);
    if (!n || name[0] == ' ' || name[n - 1] == ' ' || strchr(name, '"'))
        return 0;
    return 1;
}
static int generate(const char *ini, char *layout, const char *current, int mode, int follow)
{
    char path[MAX_PATH], full[MAX_PATH], error[200], data[2048], row[100], status[200], value[16], copy[4096],
         *fields[FIELDS], *slash;
    WALL_PALETTE palette;
    int i, margins;
    GetPrivateProfileStringA("Desktop", "Wallpaper", "WALL.BMP", path, sizeof(path), ini);
    if (path[0] == '\\' || (strlen(path) > 1 && path[1] == ':'))
        lstrcpynA(full, path, sizeof(full));
    else
    {
        lstrcpynA(full, ini, sizeof(full));
        slash = strrchr(full, '\\');
        if (!slash || strlen(full) - (strlen(slash + 1)) + strlen(path) >= sizeof(full))
            return 0;
        strcpy(slash + 1, path);
    }
    strcpy(copy, layout);
    if (!split(copy, fields, FIELDS))
        return 0;
    margins = atoi(fields[4]) == 0 || atoi(fields[4]) == 3;
    if (!wall_analyze(full, mode, margins, &palette, error, sizeof(error)))
    {
        WritePrivateProfileStringA("AutoTheme", "Status", error, ini);
        return 0;
    }
    sprintf(data, "1|%d|%06lX|%06lX|%06lX", GetPrivateProfileIntA("Options", "meterColors", 0, ini) != 0, palette.low,
            palette.mid, palette.high);
    for (i = 0; i < COUNT; i++)
    {
        sprintf(row, "|%06lX|%06lX|%06lX|%d", palette.bg, palette.fg, palette.accent, palette.alpha);
        strcat(data, row);
    }
    if (lstrcmpiA(current, data) && (!put(ini, 1000, "Previous appearance", current) || !apply(ini, layout, data)))
        return 0;
    sprintf(status, "%s palette, %d%% opacity. Sampled text contrast %.1f:1.%s", palette.light ? "Light" : "Dark",
            palette.alpha, palette.contrast, palette.busy ? " Detailed image: extra coverage." : "");
    if (!put(ini, 1001, palette.light ? "Wallpaper - light" : "Wallpaper - dark", data) ||
            !WritePrivateProfileStringA("Desktop", "ThemeActive", "1001", ini) ||
            !WritePrivateProfileStringA("AutoTheme", "Status", status, ini) ||
            !WritePrivateProfileStringA("AutoTheme", "Image", full, ini))
        return 0;
    sprintf(value, "%d", mode);
    if (!WritePrivateProfileStringA("AutoTheme", "Mode", value, ini))
        return 0;
    return WritePrivateProfileStringA("AutoTheme", "Follow", follow ? "1" : "0", ini) != 0;
}
int theme_action(const char *cmd, const char *ini, char *layout)
{
    char data[2048], current[2048], name[80], other[80], sec[32], idtext[16], meter[96], *parts[4], *p;
    int id = -1, i, ok = 0;
    char tail;
    if (!capture(ini, layout, current))
        return 0;
    if (sscanf(cmd, "auto/%d/%d%c", &id, &i, &tail) == 2)
    {
        if (id < 0 || id > 2 || i < 0 || i > 1)
            return 0;
        ok = generate(ini, layout, current, id, i);
    }
    else if (!strcmp(cmd, "refresh"))
    {
        if (!GetPrivateProfileIntA("AutoTheme", "Follow", 0, ini))
            return 1;
        id = GetPrivateProfileIntA("AutoTheme", "Mode", 0, ini);
        if (id < 0 || id > 2)
            id = 0;
        ok = generate(ini, layout, current, id, 1);
    }
    else if (!strcmp(cmd, "unfollow"))
    {
        ok = WritePrivateProfileStringA("AutoTheme", "Follow", "0", ini);
    }
    else if (sscanf(cmd, "apply/%d%c", &id, &tail) == 1)
    {
        if (!load(ini, id, name, data))
            return 0;
        if (lstrcmpiA(current, data))
        {
            if (!put(ini, 1000, "Previous appearance", current) || !apply(ini, layout, data))
                return 0;
        }
        sprintf(idtext, "%d", id == 1000 ? -1 : id);
        ok = WritePrivateProfileStringA("Desktop", "ThemeActive", idtext, ini) &&
             WritePrivateProfileStringA("AutoTheme", "Follow", "0", ini);
    }
    else if (!strncmp(cmd, "new/", 4))
    {
        if (!decode_name(cmd + 4, name))
            return 0;
        for (i = 0; i < CUSTOM; i++)
        {
            section(100 + i, sec);
            GetPrivateProfileStringA(sec, "Name", "", other, sizeof(other), ini);
            if (*other && !lstrcmpiA(name, other))
                return 0;
            if (!*other && id < 0)
                id = 100 + i;
        }
        if (id < 0)
            return 0;
        ok = put(ini, id, name, current);
        sprintf(idtext, "%d", id);
        if (ok)
            ok = WritePrivateProfileStringA("Desktop", "ThemeActive", idtext, ini);
    }
    else if (sscanf(cmd, "update/%d%c", &id, &tail) == 1)
    {
        if (id < 100 || id >= 100 + CUSTOM || !load(ini, id, name, data))
            return 0;
        ok = put(ini, id, name, current);
        sprintf(idtext, "%d", id);
        if (ok)
            ok = WritePrivateProfileStringA("Desktop", "ThemeActive", idtext, ini);
    }
    else if (sscanf(cmd, "delete/%d%c", &id, &tail) == 1)
    {
        if (id < 100 || id >= 100 + CUSTOM)
            return 0;
        section(id, sec);
        ok = WritePrivateProfileStringA(sec, NULL, NULL, ini);
    }
    else if (!strncmp(cmd, "meter/", 6))
    {
        if (strlen(cmd + 6) >= sizeof(meter))
            return 0;
        strcpy(meter, cmd + 6);
        for (p = meter; *p; p++)
            if (*p == ',')
                *p = '|';
        if (!split(meter, parts, 4) || (strcmp(parts[0], "0") && strcmp(parts[0], "1")) || !color(parts[1]) ||
                !color(parts[2]) || !color(parts[3]))
            return 0;
        ok = WritePrivateProfileStringA("Options", "meterColors", parts[0], ini) &&
             WritePrivateProfileStringA("Options", "meterLow", parts[1], ini) &&
             WritePrivateProfileStringA("Options", "meterMid", parts[2], ini) &&
             WritePrivateProfileStringA("Options", "meterHigh", parts[3], ini);
    }
    if (ok)
    {
        sprintf(idtext, "%lu", GetTickCount());
        ok = WritePrivateProfileStringA("Desktop", "ThemeRevision", idtext, ini);
    }
    return ok;
}
static void quote(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char*)s;
    fputc('\'', f);
    for (; *p; p++)
    {
        if (*p < 32 || *p >= 127 || *p == '\'' || *p == '\\' || *p == '<')
            fprintf(f, "\\x%02x", *p);
        else
            fputc(*p, f);
    }
    fputc('\'', f);
}
void theme_publish(FILE *f, const char *ini, const char *layout)
{
    char current[2048], data[2048], name[80], value[32], status[200], path[MAX_PATH];
    int id, active = (int)GetPrivateProfileIntA("Desktop", "ThemeActive", 0, ini), first = 1;
    if (!capture(ini, layout, current) || !load(ini, active, name, data) || lstrcmpiA(current, data))
        active = -1;
    fprintf(f, "var themeActive=%d;var themeList=[", active);
    for (id = 0; id < PRESETS + CUSTOM + 2; id++)
    {
        int key = id < (int)PRESETS ? id : id < (int)PRESETS + CUSTOM ? 100 + id - PRESETS : 1000 + id - PRESETS - CUSTOM;
        if (!load(ini, key, name, data))
            continue;
        if (!first)
            fputc(',', f);
        first = 0;
        fprintf(f, "{id:%d,name:", key);
        quote(f, name);
        fputc('}', f);
    }
    fputs("];var themeRevision=", f);
    GetPrivateProfileStringA("Desktop", "ThemeRevision", "0", value, sizeof(value), ini);
    quote(f, value);
    fputs(";\n", f);
    fprintf(f, "var themeAutoMode=%d,themeAutoFollow=%d;var themeAutoStatus=", GetPrivateProfileIntA("AutoTheme", "Mode", 0,
            ini), GetPrivateProfileIntA("AutoTheme", "Follow", 0, ini) != 0);
    GetPrivateProfileStringA("AutoTheme", "Status", "", status, sizeof(status), ini);
    quote(f, status);
    fputs(";var themeAutoImage=", f);
    GetPrivateProfileStringA("AutoTheme", "Image", "", path, sizeof(path), ini);
    quote(f, path);
    fputs(";\n", f);
}
