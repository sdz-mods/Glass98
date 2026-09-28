/* Bounded settings/device commands and user-selected launch slots.
   Handles manager actions and automatic display reflow; settings persist beside EXE. */
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
int rss_fetch(const char*, const char*);
int utility_action(const char*, const char*, const char*);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "themes.h"
static char ini[MAX_PATH], js[MAX_PATH], tmp[MAX_PATH], image[MAX_PATH];
static const char *keyname = "W98WidgetsGlass";
#define PANELS 22
#define FIELDS (5+PANELS*8)
static char defaults[4096];
static void make_defaults(void)
{
    int i;
    char row[128];
    strcpy(defaults, "3|280|0|1|0");
    for (i = 0; i < PANELS; i++)
    {
        sprintf(row, "|%d|32000|%d|000000|FFFFFF|80FFFF|55|%d", i == 0, i == 0 ? 1 : 350, i == 1 ? 10 : 2);
        strcat(defaults, row);
    }
}
static int valid(char *s)
{
    char copy[4096], *p, *next;
    int i, n, field;
    if (strlen(s) >= sizeof(copy))
        return 0;
    strcpy(copy, s);
    p = copy;
    for (i = 0; i < 5 + PANELS * 8; i++)
    {
        next = strchr(p, '|');
        if (next)
            *next = 0;
        if (!*p)
            return 0;
        field = i < 5 ? -1 : (i - 5) % 8;
        if (field >= 3 && field <= 5)
        {
            if (strlen(p) != 6)
                return 0;
            for (n = 0; n < 6; n++)
                if (!isxdigit((unsigned char)p[n]))
                    return 0;
        }
        else
        {
            if (strlen(p) > 5)
                return 0;
            for (n = 0; p[n]; n++)
                if (!isdigit((unsigned char)p[n]))
                    return 0;
            n = atoi(p);
            if (i == 0 && n != 3)
                return 0;
            if (i == 1 && (n < 240 || n > 600))
                return 0;
            if ((i == 2 || i == 3 || field == 0) && n > 1)
                return 0;
            if (i == 4 && n > 3)
                return 0;
            if ((field == 1 || field == 2) && n > 32000)
                return 0;
            if (field == 6 && n > 100)
                return 0;
            if (field == 7 && (n < 1 || n > 3600))
                return 0;
        }
        if (i < 4 + PANELS * 8)
        {
            if (!next)
                return 0;
            p = next + 1;
        }
        else if (next)
            return 0;
    }
    return 1;
}
static void migrate(char *config)
{
    char old[1024], *a[19], *p;
    int i;
    char row[128], result[4096];
    if (!strncmp(config, "2|", 2))
    {
        int count = 1;
        for (p = config; *p; p++)
            if (*p == '|')
                count++;
        if (count != 109)
            return;
        config[0] = '3';
        for (i = 13; i < PANELS; i++)
        {
            sprintf(row, "|0|32000|350|000000|FFFFFF|80FFFF|55|2");
            strcat(config, row);
        }
        if (valid(config))
            WritePrivateProfileStringA("Desktop", "Layout", config, ini);
        return;
    }
    if (strncmp(config, "1|", 2) || strlen(config) >= sizeof(old))
        return;
    strcpy(old, config);
    p = old;
    for (i = 0; i < 19; i++)
    {
        a[i] = p;
        p = strchr(p, '|');
        if (i < 18 && !p)
            return;
        if (p)
            *p++ = 0;
    }
    strcpy(result, defaults);
    sprintf(result, "3|%d|%d|%d|%d", atoi(a[1]) < 240 ? 240 : atoi(a[1]), atoi(a[2]), atoi(a[3]), atoi(a[18]));
    sprintf(row, "|1|%d|%d|%.6s|%.6s|%.6s|%d|2", atoi(a[5]), atoi(a[6]), a[7], a[8], a[9], atoi(a[10]));
    strcat(result, row);
    for (i = 1; i < PANELS; i++)
    {
        sprintf(row, "|%d|32000|350|000000|FFFFFF|80FFFF|55|%d", 0, i == 1 ? 10 : 2);
        strcat(result, row);
    }
    if (valid(result))
    {
        strcpy(config, result);
        WritePrivateProfileStringA("Desktop", "Layout", config, ini);
    }
}
#include "layouts.h"
static const char *optionKeys[] = {"drives", "adapters", "clock", "reminders", "rss", "rssminutes", "cd", "launch0", "launch1", "launch2", "launch3", "launch4", "launch5", "label0", "label1", "label2", "label3", "label4", "label5", "globalSeconds", "worlds", "notes", "timerMinutes", "timerMode", "timerState", "timerSound", "eventName", "eventDate", "pingHost", "photoSeconds", "photo0", "photo1", "photo2", "photo3", "photo4", "photo5", "fav0", "fav1", "fav2", "fav3", "meterColors", "meterLow", "meterMid", "meterHigh", "disableAlpha", NULL};
static int writable_option(int i)
{
    return i < 7 || (i >= 13 && i < 30) || i == 44;
}
static int save_options(char *s)
{
    char *end, *equals, key[32], raw[1024], text[1024], stored[2048];
    WCHAR wide[1024];
    unsigned int byte;
    int i, n, j, k;
    while (s && *s)
    {
        end = strchr(s, ';');
        if (end)
            *end = 0;
        equals = strchr(s, '=');
        if (!equals || equals - s > 30)
            return 0;
        memcpy(key, s, equals - s);
        key[equals - s] = 0;
        for (i = 0; optionKeys[i]; i++)
            if (!strcmp(key, optionKeys[i]))
                break;
        if (!optionKeys[i] || !writable_option(i))
            return 0;
        s = equals + 1;
        n = strlen(s);
        if (n > 1800 || n % 2)
            return 0;
        for (j = 0; j < n; j += 2)
        {
            if (!isxdigit(s[j]) || !isxdigit(s[j + 1]) || sscanf(s + j, "%2x", &byte) != 1 || !byte)
                return 0;
            raw[j / 2] = (char)byte;
        }
        raw[n / 2] = 0;
        if (!MultiByteToWideChar(CP_UTF8, 0, raw, -1, wide, 1024) ||
                !WideCharToMultiByte(CP_ACP, 0, wide, -1, text, 1024, NULL, NULL))
            return 0;
        for (j = 0, k = 0; text[j] && k < 2000; j++)
        {
            if (text[j] == '\r')
                continue;
            if (text[j] == '\n')
            {
                if (i != 3 && i != 20 && i != 21)
                    return 0;
                stored[k++] = '\\';
                stored[k++] = 'n';
            }
            else
                stored[k++] = text[j];
        }
        stored[k] = 0;
        if (i >= 13 && i <= 18 && strlen(stored) > 80)
            return 0;
        if (i == 19)
        {
            if (!*stored || strlen(stored) > 4)
                return 0;
            for (j = 0; stored[j]; j++)
                if (!isdigit((unsigned char)stored[j]))
                    return 0;
            if (atoi(stored) > 3600)
                return 0;
        }
        if (i == 44 && strcmp(stored, "0") && strcmp(stored, "1"))
            return 0;
        if (i == 28)
        {
            if (strlen(stored) > 120)
                return 0;
            for (j = 0; stored[j]; j++)
                if (!isalnum((unsigned char)stored[j]) && stored[j] != '.' && stored[j] != '-')
                    return 0;
        }
        if (!WritePrivateProfileStringA("Options", key, stored, ini))
            return 0;
        s = end ? end + 1 : NULL;
    }
    return 1;
}
static int panel_save(int index, char *row)
{
    char current[4096], result[4096], *a[FIELDS], *r[13], *p;
    int i, j, all;
    if (index < 0 || index >= PANELS || strlen(row) > 180)
        return 0;
    p = row;
    for (i = 0; i < 13; i++)
    {
        r[i] = p;
        p = strchr(p, ',');
        if (i < 12 && !p)
            return 0;
        if (p)
            *p++ = 0;
    }
    if (p || strlen(r[12]) != 1 || (r[12][0] != '0' && r[12][0] != '1'))
        return 0;
    all = atoi(r[12]);
    GetPrivateProfileStringA("Desktop", "Layout", defaults, current, sizeof(current), ini);
    migrate(current);
    if (!valid(current))
        strcpy(current, defaults);
    p = current;
    for (i = 0; i < FIELDS; i++)
    {
        a[i] = p;
        p = strchr(p, '|');
        if (p)
            *p++ = 0;
    }
    /* A stale manager must not re-request placement after the desktop has already
       committed it. Off->on is the only operation that requests a new position. */
    if (atoi(a[5 + index * 8]) && atoi(r[4]) && !strcmp(r[5], "32000") && !strcmp(r[6], "32000"))
    {
        r[5] = a[6 + index * 8];
        r[6] = a[7 + index * 8];
    }
    sprintf(result, "3|%s|%s|%s|%s", r[0], r[1], r[2], r[3]);
    for (i = 0; i < PANELS; i++)
        for (j = 0; j < 8; j++)
        {
            strcat(result, "|");
            strcat(result, i == index ? r[4 + j] : all && j >= 3 && j <= 6 ? r[4 + j] : a[5 + i * 8 + j]);
        }
    if (!valid(result) || !WritePrivateProfileStringA("Desktop", "Layout", result, ini))
        return 0;
    layout_clear_hidden(index);
    /* A fit/center change can reveal dark margins around a light image. */
    if (strcmp(a[4], r[3]) && GetPrivateProfileIntA("AutoTheme", "Follow", 0, ini))
        theme_action("refresh", ini, result);
    return 1;
}
static int auto_place(const char *args)
{
    char config[4096], result[4096], *a[FIELDS], *p, text[32], tail;
    int index, x, y, w, crowded, i;
    if (sscanf(args, "%d/%d/%d/%d/%d%c", &index, &x, &y, &w, &crowded, &tail) != 5 || index < 0 || index >= PANELS ||
            index == 5 || index == 21 || x < 1 || y < 1 || x > 32000 || y > 32000 || w < 240 || w > 600 || crowded < 0 ||
            crowded > 1)
        return 0;
    GetPrivateProfileStringA("Desktop", "Layout", defaults, config, sizeof(config), ini);
    if (!valid(config))
        return 0;
    p = config;
    for (i = 0; i < FIELDS; i++)
    {
        a[i] = p;
        p = strchr(p, '|');
        if (p)
            *p++ = 0;
    }
    /* Compare-and-set coordinates only. Never overwrite colors, options, another
       widget, a manual move, a disabled widget or a changed width. No ACK.JS write. */
    if (!atoi(a[5 + index * 8]) || atoi(a[1]) != w || strcmp(a[6 + index * 8], "32000") ||
            strcmp(a[7 + index * 8], "32000"))
        return 1;
    result[0] = 0;
    for (i = 0; i < FIELDS; i++)
    {
        if (i)
            strcat(result, "|");
        if (crowded && i == 5 + index * 8)
            strcat(result, "0");
        else if (i == 6 + index * 8 || i == 7 + index * 8)
        {
            sprintf(text, "%d", i == 6 + index * 8 ? x : y);
            strcat(result, text);
        }
        else
            strcat(result, a[i]);
    }
    if (!valid(result) || !WritePrivateProfileStringA("Desktop", "Layout", result, ini))
        return 0;
    {
        char hidden[PANELS + 1];
        layout_hidden(hidden);
        hidden[index] = crowded ? '1' : '0';
        if (!WritePrivateProfileStringA("Desktop", "LayoutHidden", hidden, ini))
            return 0;
    }
    return WritePrivateProfileStringA("Desktop", "PlacementNotice",
                                      crowded ? "No free space in this resolution. Disable another widget, then enable this one." : "", ini) != 0;
}
static void quoted(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char*)s;
    fputc('\'', f);
    for (; *p; p++)
    {
        if (*p < 32 || *p >= 127)
            fprintf(f, "\\x%02x", *p);
        else
        {
            if (*p == '\'' || *p == '\\')
                fputc('\\', f);
            fputc(*p, f);
        }
    }
    fputc('\'', f);
}
static int publish(void)
{
    char config[4096], wall[MAX_PATH], settings[MAX_PATH + 10], value[2048], *p;
    FILE *f;
    int bad, i;
    if (!layout_sync())
        return 0;
    /* Win9x caches profile writes: flush before reporting a durable save. */
    WritePrivateProfileStringA(NULL, NULL, NULL, ini);
    GetPrivateProfileStringA("Desktop", "Layout", defaults, config, sizeof(config), ini);
    migrate(config);
    if (!valid(config))
        strcpy(config, defaults);
    GetPrivateProfileStringA("Desktop", "Wallpaper", "WALL.BMP", wall, sizeof(wall), ini);
    f = fopen(tmp, "wb");
    if (!f)
        return 0;
    strcpy(settings, "file:///");
    strcat(settings, image);
    strcpy(strrchr(settings, '\\') + 1, "SETTINGS.HTM");
    for (p = settings; *p; p++)
        if (*p == '\\')
            *p = '/';
    fputs("var savedLayout=", f);
    quoted(f, config);
    fputs(";var savedWallpaper=", f);
    quoted(f, wall);
    fputs(";var settingsURL=", f);
    quoted(f, settings);
    fputs(";\r\n", f);
    fputs("var savedOptions={", f);
    for (i = 0; optionKeys[i]; i++)
    {
        if (i)
            fputc(',', f);
        quoted(f, optionKeys[i]);
        fputc(':', f);
        GetPrivateProfileStringA("Options", optionKeys[i],
                                 i == 0 ? "C" : i == 1 ? "*" : i == 2 ? "digital24" : i == 5 ? "15" : i == 19 || i == 40 ||
                                 i == 44 ? "0" : i == 41 ? "62D98B" : i == 42 ? "FFD166" : i == 43 ? "FF6262" : "", value, sizeof(value), ini);
        quoted(f, value);
    }
    fputs("};\n", f);
    theme_publish(f, ini, config);
    GetPrivateProfileStringA("Desktop", "Resolution", "", value, sizeof(value), ini);
    fputs("var layoutResolution=", f);
    quoted(f, value);
    fprintf(f, ";var layoutWorkWidth=%d;var layoutWorkHeight=%d;", GetPrivateProfileIntA("Desktop", "WorkWidth", 0, ini),
            GetPrivateProfileIntA("Desktop", "WorkHeight", 0, ini));
    memset(value, 0, sizeof(value));
    layout_hidden(value);
    fputs("var layoutHidden=", f);
    quoted(f, value);
    fprintf(f, ";var layoutReflow=%d;\n", GetPrivateProfileIntA("Desktop", "Reflow", 0, ini));
    GetPrivateProfileStringA("Desktop", "PlacementNotice", "", value, sizeof(value), ini);
    fputs("var placementNotice=", f);
    quoted(f, value);
    fputs(";\n", f);
    bad = ferror(f);
    if (fclose(f))
        bad = 1;
    if (bad)
        return 0;
    if (!DeleteFileA(js) && GetLastError() != ERROR_FILE_NOT_FOUND)
        return 0;
    bad = MoveFileA(tmp, js) != 0;
    if (bad)
    {
        HWND writer = FindWindowA("W98WidgetsDataWriter", NULL);
        if (writer)
            PostMessageA(writer, WM_APP + 1, 0, 0);
    }
    return bad;
}
static int regvalue(const char *path, const char *name, const char *value)
{
    HKEY k;
    DWORD d;
    LONG e = RegCreateKeyExA(HKEY_CLASSES_ROOT, path, 0, NULL, 0, KEY_WRITE, NULL, &k, &d);
    if (e)
        return 0;
    e = RegSetValueExA(k, name, 0, REG_SZ, (BYTE*)value, strlen(value) + 1);
    RegCloseKey(k);
    return !e;
}
static int registration(int add)
{
    char expected[MAX_PATH + 12], old[MAX_PATH + 12];
    HKEY k;
    DWORD size = sizeof(old), type;
    LONG e;
    sprintf(expected, "\"%s\" \"%%1\"", image);
    e = RegOpenKeyExA(HKEY_CLASSES_ROOT, "W98WidgetsGlass\\shell\\open\\command", 0, KEY_READ, &k);
    if (!e)
    {
        e = RegQueryValueExA(k, NULL, NULL, &type, (BYTE*)old, &size);
        RegCloseKey(k);
        old[sizeof(old) - 1] = 0;
        if (e || type != REG_SZ || lstrcmpiA(old, expected))
            return 0;
    }
    else if (e != ERROR_FILE_NOT_FOUND)
        return 0;
    if (add)
        return regvalue(keyname, NULL, "URL:Glass98 settings") && regvalue(keyname, "URL Protocol", "") &&
               regvalue("W98WidgetsGlass\\shell\\open\\command", NULL, expected) && publish();
    RegDeleteKeyA(HKEY_CLASSES_ROOT, "W98WidgetsGlass\\shell\\open\\command");
    RegDeleteKeyA(HKEY_CLASSES_ROOT, "W98WidgetsGlass\\shell\\open");
    RegDeleteKeyA(HKEY_CLASSES_ROOT, "W98WidgetsGlass\\shell");
    RegDeleteKeyA(HKEY_CLASSES_ROOT, keyname);
    return 1;
}
int WINAPI WinMain(HINSTANCE h, HINSTANCE prev, LPSTR raw, int show)
{
    char ack[8192], ackpath[MAX_PATH], acktemp[MAX_PATH], transfer[4096], chunk[1800], command[8192], decoded[8192],
         directory[MAX_PATH], value[1024], key[32], *p, *q, *slash, *layout;
    char tail;
    unsigned int c;
    int action = 0, ok = 0, slot, i;
    HWND window;
    COPYDATASTRUCT data;
    DWORD result;
    FILE *ackfile;
    BROWSEINFOA bi;
    LPITEMIDLIST item;
    HANDLE mutex;
    OPENFILENAMEA ofn;
    char selected[MAX_PATH], working[MAX_PATH];
    (void)h;
    (void)prev;
    (void)show;
    if (!GetModuleFileNameA(NULL, image, sizeof(image)) || strlen(image) > MAX_PATH - 16)
        return 1;
    strcpy(ini, image);
    slash = strrchr(ini, '\\');
    if (!slash)
        return 1;
    strcpy(slash + 1, "WIDGETS.INI");
    make_defaults();
    defaults[10] = (char)('0' + GetPrivateProfileIntA("Desktop", "InitialWallpaperMode", 0, ini) % 4);
    strcpy(directory, image);
    *strrchr(directory, '\\') = 0;
    strcpy(js, ini);
    strcpy(strrchr(js, '\\') + 1, "PREFS.JS");
    strcpy(tmp, ini);
    strcpy(strrchr(tmp, '\\') + 1, "PREFS.TMP");
    if (strlen(raw) >= sizeof(command))
        return 2;
    strcpy(command, raw);
    p = command;
    if (*p == '"')
    {
        p++;
        q = strrchr(p, '"');
        if (!q || q[1])
            return 2;
        *q = 0;
    }
    q = decoded;
    while (*p)
    {
        if (*p == '%')
        {
            if (strlen(p) < 3 || !isxdigit(p[1]) || !isxdigit(p[2]) || sscanf(p + 1, "%2x", &c) != 1 || !c)
                return 2;
            *q++ = (char)c;
            p += 3;
        }
        else *q++ = *p++;
    }
    *q = 0;
    strcpy(ack, decoded);
    mutex = CreateMutexA(NULL, FALSE, "W98WidgetsSettingsWrite");
    if (!mutex)
        return 3;
    if (WaitForSingleObject(mutex, 10000) != WAIT_OBJECT_0)
    {
        CloseHandle(mutex);
        return 3;
    }
    if (!strcmp(decoded, "register"))
        ok = registration(1);
    else if (!strcmp(decoded, "unregister"))
        ok = registration(0);
    else if (!strcmp(decoded, "publish"))
        ok = publish();
    else if (!strcmp(decoded, "prepare"))
    {
        GetPrivateProfileStringA("Desktop", "Layout", defaults, transfer, sizeof(transfer), ini);
        migrate(transfer);
        if (!valid(transfer))
            strcpy(transfer, defaults);
        ok = WritePrivateProfileStringA("Desktop", "Layout", transfer, ini) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:arrange/", 24))
    {
        ok = (!layout_guard(decoded + 16) || WritePrivateProfileStringA("Desktop", "Reflow", "1", ini)) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:display/", 24))
    {
        ok = layout_display(decoded + 24) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:autoplace/", 26))
    {
        ok = (!layout_guard(decoded + 26) || auto_place(decoded + 26)) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:theme/", 22))
    {
        GetPrivateProfileStringA("Desktop", "Layout", defaults, transfer, sizeof(transfer), ini);
        migrate(transfer);
        if (valid(transfer))
        {
            ok = theme_action(decoded + 22, ini, transfer);
            if (!publish())
                ok = 0;
        }
    }
    else if (!strncmp(decoded, "w98widgetsglass:panel/", 22) || !strncmp(decoded, "w98widgetsglass:panelbrowse/", 28))
    {
        action = !strncmp(decoded, "w98widgetsglass:panelbrowse/", 28) ? 2 : 1;
        p = decoded + (action == 2 ? 28 : 22);
        q = strchr(p, '/');
        if (q && q > p && q - p < 3 && isdigit((unsigned char)p[0]) && (q - p == 1 || isdigit((unsigned char)p[1])))
        {
            *q = 0;
            ok = (!layout_guard(q + 1) || panel_save(atoi(p), q + 1)) && publish();
        }
    }
    else if (!strncmp(decoded, "w98widgetsglass:options/", 24))
    {
        ok = save_options(decoded + 24) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:begin/", 22))
    {
        p = decoded + 22;
        for (i = 0; optionKeys[i]; i++)
            if (writable_option(i) && !strcmp(p, optionKeys[i]))
            {
                WritePrivateProfileStringA("Transfer", "Key", p, ini);
                ok = WritePrivateProfileStringA("Transfer", "Hex", "", ini);
                break;
            }
    }
    else if (!strncmp(decoded, "w98widgetsglass:part/", 21))
    {
        p = decoded + 21;
        GetPrivateProfileStringA("Transfer", "Hex", "", transfer, sizeof(transfer), ini);
        if (strlen(p) <= 128 && strlen(transfer) + strlen(p) < 1801)
        {
            strcat(transfer, p);
            ok = WritePrivateProfileStringA("Transfer", "Hex", transfer, ini);
        }
    }
    else if (!strcmp(decoded, "w98widgetsglass:commit"))
    {
        GetPrivateProfileStringA("Transfer", "Key", "", key, sizeof(key), ini);
        GetPrivateProfileStringA("Transfer", "Hex", "", transfer, sizeof(transfer), ini);
        sprintf(command, "%s=%s", key, transfer);
        ok = save_options(command) && publish();
        WritePrivateProfileStringA("Transfer", NULL, NULL, ini);
    }
    else if (!strncmp(decoded, "w98widgetsglass:device/", 23))
    {
        p = decoded + 23;
        window = FindWindowA("W98WidgetsDataWriter", NULL);
        if (window && strlen(p) < 127)
        {
            data.dwData = 0x9832;
            data.cbData = strlen(p) + 1;
            data.lpData = p;
            SendMessageTimeoutA(window, WM_COPYDATA, 0, (LPARAM)&data, SMTO_ABORTIFHUNG, 2000, &result);
            ok = 1;
        }
    }
    else if (!strcmp(decoded, "w98widgetsglass:rss"))
    {
        action = 3;
        ok = 1;
    }
    else if (sscanf(decoded, "w98widgetsglass:choose/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        action = 4;
        ok = 1;
    }
    else if (sscanf(decoded, "w98widgetsglass:folder/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        action = 5;
        ok = 1;
    }
    else if (sscanf(decoded, "w98widgetsglass:launch/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "launch%d", slot);
        GetPrivateProfileStringA("Options", key, "", value, sizeof(value), ini);
        if (*value)
        {
            lstrcpynA(working, value, sizeof(working));
            p = strrchr(working, '\\');
            if (p)
            {
                if (p == working + 2)
                    p[1] = 0;
                else *p = 0;
            }
            else
                working[0] = 0;
            /* Shortcuts retain their configured Start in directory. Direct files start beside themselves. */
            p = strrchr(value, '.');
            if (p && !lstrcmpiA(p, ".lnk"))
                working[0] = 0;
            ok = (UINT)ShellExecuteA(NULL, "open", value, NULL, *working ? working : NULL, SW_SHOWNORMAL) > 32;
        }
    }
    else if (sscanf(decoded, "w98widgetsglass:clear/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "launch%d", slot);
        ok = WritePrivateProfileStringA("Options", key, "", ini) && publish();
    }
    else if (!strncmp(decoded, "w98widgetsglass:util/", 21))
    {
        action = 6;
        ok = 1;
    }
    else
    {
        if (!strncmp(decoded, "w98widgetsglass:save/", 21))
        {
            action = 1;
            layout = decoded + 21;
        }
        else if (!strncmp(decoded, "w98widgetsglass:browse/", 23))
        {
            action = 2;
            layout = decoded + 23;
        }
        else
            layout = NULL;
        p = NULL;
        if (layout)
        {
            p = strchr(layout, '~');
            if (p)
                *p++ = 0;
        }
        if (layout && valid(layout))
        {
            ok = (!p || save_options(p)) && WritePrivateProfileStringA("Desktop", "Layout", layout, ini) && publish();
        }
        else
        {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
            return 2;
        }
    }
    if (!strncmp(ack, "w98widgetsglass:arrange/", 24) || !strncmp(ack, "w98widgetsglass:theme/", 22) ||
            !strncmp(ack, "w98widgetsglass:panel", 21) || !strncmp(ack, "w98widgetsglass:options/", 24) ||
            !strncmp(ack, "w98widgetsglass:begin/", 22) || !strncmp(ack, "w98widgetsglass:part/", 21) ||
            !strcmp(ack, "w98widgetsglass:commit"))
    {
        sprintf(ackpath, "%s\\ACK.JS", directory);
        sprintf(acktemp, "%s\\ACK.TMP", directory);
        ackfile = fopen(acktemp, "wb");
        if (ackfile)
        {
            fputs("var actionAck=", ackfile);
            quoted(ackfile, ack + 16);
            fprintf(ackfile, ";var ackTime=%lu;var ackOK=%d;\n", GetTickCount(), ok);
            i = ferror(ackfile);
            if (fclose(ackfile))
                i = 1;
            if (!i && (DeleteFileA(ackpath) || GetLastError() == ERROR_FILE_NOT_FOUND))
                MoveFileA(acktemp, ackpath);
        }
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    if (ok && action == 6)
    {
        ok = utility_action(decoded + 21, ini, directory);
        if (ok)
        {
            mutex = CreateMutexA(NULL, FALSE, "W98WidgetsSettingsWrite");
            if (mutex && WaitForSingleObject(mutex, 10000) == WAIT_OBJECT_0)
            {
                ok = publish();
                ReleaseMutex(mutex);
            }
            else
                ok = 0;
            if (mutex)
                CloseHandle(mutex);
        }
    }
    if (ok && action == 3)
    {
        HANDLE fetchLock = CreateMutexA(NULL, FALSE, "W98WidgetsRssFetch");
        if (fetchLock && WaitForSingleObject(fetchLock, 0) == WAIT_OBJECT_0)
        {
            rss_fetch(ini, directory);
            ReleaseMutex(fetchLock);
        }
        if (fetchLock)
            CloseHandle(fetchLock);
    }
    if (ok && (action == 2 || action == 4 || action == 5))
    {
        CoInitialize(NULL);
        memset(&ofn, 0, sizeof(ofn));
        selected[0] = 0;
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = selected;
        ofn.nMaxFile = sizeof(selected);
        ofn.lpstrFilter = "Wallpaper images (*.bmp;*.jpg;*.jpeg;*.gif)\0*.bmp;*.jpg;*.jpeg;*.gif\0\0";
        ofn.lpstrTitle = "Choose widget desktop wallpaper";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (action == 4)
        {
            ofn.Flags |= OFN_NODEREFERENCELINKS;
            ofn.lpstrFilter = "Programs and shortcuts\0*.exe;*.com;*.bat;*.lnk;*.cpl\0All files\0*.*\0\0";
            ofn.lpstrTitle = "Choose a Quick Launch item";
        }
        i = 0;
        if (action == 5)
        {
            memset(&bi, 0, sizeof(bi));
            bi.lpszTitle = "Choose a Quick Launch folder";
            bi.ulFlags = BIF_RETURNONLYFSDIRS;
            item = SHBrowseForFolderA(&bi);
            if (item)
            {
                i = SHGetPathFromIDListA(item, selected);
                CoTaskMemFree(item);
            }
        }
        else
            i = GetOpenFileNameA(&ofn);
        if (i)
        {
            mutex = CreateMutexA(NULL, FALSE, "W98WidgetsSettingsWrite");
            if (mutex && WaitForSingleObject(mutex, 10000) == WAIT_OBJECT_0)
            {
                if (action == 2)
                {
                    ok = WritePrivateProfileStringA("Desktop", "Wallpaper", selected, ini);
                    if (ok)
                    {
                        GetPrivateProfileStringA("Desktop", "Layout", defaults, transfer, sizeof(transfer), ini);
                        migrate(transfer);
                        ok = valid(transfer) && theme_action("refresh", ini, transfer);
                        if (!publish())
                            ok = 0;
                    }
                }
                else
                {
                    sprintf(key, "launch%d", slot);
                    ok = WritePrivateProfileStringA("Options", key, selected, ini) && publish();
                }
                ReleaseMutex(mutex);
            }
            else
                ok = 0;
            if (mutex)
                CloseHandle(mutex);
        }
    }
    if (!ok && (action == 2 || !strncmp(decoded, "w98widgetsglass:theme/auto/", 27)))
    {
        GetPrivateProfileStringA("AutoTheme", "Status", "Could not generate the wallpaper theme.", value, sizeof(value), ini);
        MessageBoxA(NULL, value, "Wallpaper theme", MB_OK | MB_ICONEXCLAMATION);
    }
    else if (!ok)
        MessageBoxA(NULL, action == 6 ?
                    "The action could not be completed. Check that the selected file or device is still available." :
                    "Could not save widget settings. Check that the installation folder is writable and that another installation does not own the settings handler.",
                    "Glass98", MB_OK | MB_ICONEXCLAMATION);
    return ok ? 0 : 1;
}
