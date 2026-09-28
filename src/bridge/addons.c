/* Optional Win9x collectors. Disabled widgets release their counters/helpers. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extras.h"
#include "addons.h"
static int diskStarted[2];
static DWORD diskLast[2], diskTick, resourceRetry;
static const char *diskNames[] = {"VFAT\\BReadsSec", "VFAT\\BWritesSec"};
static LONG stat_read(const char *group, int index, DWORD *v)
{
    HKEY k;
    DWORD size = 4, type;
    LONG e;
    char path[80];
    sprintf(path, "PerfStats\\%s", group);
    e = RegOpenKeyExA(HKEY_DYN_DATA, path, 0, KEY_READ, &k);
    if (e)
        return e;
    e = RegQueryValueExA(k, diskNames[index], NULL, &type, (BYTE*)v, &size);
    RegCloseKey(k);
    return !e && size != 4 ? ERROR_INVALID_DATA : e;
}
static void disk_activity(FILE *f)
{
    DWORD tick = GetTickCount(), v[2], delta;
    double rate[2] = {-1, -1};
    int j, valid = 1;
    for (j = 0; j < 2; j++)
    {
        if (!diskStarted[j])
        {
            diskStarted[j] = !stat_read("StartStat", j, &v[j]);
            diskTick = 0;
        }
        if (!diskStarted[j] || stat_read("StatData", j, &v[j]))
            valid = 0;
    }
    if (valid)
    {
        if (diskTick && tick != diskTick)
            for (j = 0; j < 2; j++)
            {
                delta = v[j] - diskLast[j];
                rate[j] = delta * 1000.0 / (tick - diskTick);
                if (rate[j] > 2147483648.0)
                    rate[j] = -1;
            }
        for (j = 0; j < 2; j++)
            diskLast[j] = v[j];
        diskTick = tick;
    }
    else
        diskTick = 0;
    fprintf(f, "var diskActivity={read:%.0f,write:%.0f};\n", rate[0], rate[1]);
}
static void resources(FILE *f, const char *root)
{
    HWND w = FindWindowA("W98Resources16", NULL);
    DWORD value = 0;
    char command[MAX_PATH + 20];
    if (!w && (!resourceRetry || GetTickCount() - resourceRetry > 60000))
    {
        resourceRetry = GetTickCount();
        sprintf(command, "\"%s\\RSRC16.EXE\"", root);
        WinExec(command, SW_HIDE);
    }
    if (w)
        SendMessageTimeoutA(w, WM_USER + 98, 0, 0, SMTO_ABORTIFHUNG, 300, &value);
    fprintf(f, "var resources={system:%d,user:%d,gdi:%d};\n", value & 0x01000000 ? (int)(value & 255) : -1,
            value & 0x01000000 ? (int)((value >> 8) & 255) : -1, value & 0x01000000 ? (int)((value >> 16) & 255) : -1);
}
static DWORD name_id(const char *s)
{
    DWORD h = 2166136261UL;
    for (; *s; s++)
    {
        h ^= (unsigned char) * s;
        h *= 16777619UL;
    }
    return h;
}
static void recent(FILE *f)
{
    char folder[MAX_PATH], pattern[MAX_PATH], *ext;
    LPITEMIDLIST item;
    WIN32_FIND_DATAA data, rows[8], swap;
    HANDLE search;
    int count = 0, j, k;
    fputs("var recentFiles=[", f);
    if (SHGetSpecialFolderLocation(NULL, CSIDL_RECENT, &item) != NOERROR)
        goto done;
    if (!SHGetPathFromIDListA(item, folder))
    {
        CoTaskMemFree(item);
        goto done;
    }
    CoTaskMemFree(item);
    if (strlen(folder) > MAX_PATH - 8)
        goto done;
    sprintf(pattern, "%s\\*.lnk", folder);
    search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE)
        goto done;
    do
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        for (j = 0; j < count; j++)
            if (CompareFileTime(&data.ftLastWriteTime, &rows[j].ftLastWriteTime) > 0)
                break;
        if (j >= 8)
            continue;
        if (count < 8)
            count++;
        for (k = count - 1; k > j; k--)
            rows[k] = rows[k - 1];
        rows[j] = data;
    }
    while (FindNextFileA(search, &data));
    FindClose(search);
    for (j = 0; j < count; j++)
    {
        if (j)
            fputc(',', f);
        fprintf(f, "{id:%lu,name:", name_id(rows[j].cFileName));
        ext = strrchr(rows[j].cFileName, '.');
        if (ext)
            *ext = 0;
        js_string(f, rows[j].cFileName);
        fputc('}', f);
    }
done:
    fputs("];\n", f);
}
static void media(FILE *f)
{
    DWORD mask = GetLogicalDrives(), i, type, serial, flags, comp;
    char path[] = "C:\\", label[80];
    int first = 1, ready;
    fputs("var mediaDrives=[", f);
    for (i = 2; i < 26; i++)
        if (mask & (1UL << i))
        {
            path[0] = (char)('A' + i);
            type = GetDriveTypeA(path);
            if (type != DRIVE_REMOVABLE && type != DRIVE_CDROM)
                continue;
            label[0] = 0;
            ready = GetVolumeInformationA(path, label, sizeof(label), &serial, &comp, &flags, NULL, 0) != 0;
            if (!first)
                fputc(',', f);
            first = 0;
            fprintf(f, "{id:'%c',type:%lu,ready:%d,label:", path[0], type, ready);
            js_string(f, label);
            fputc('}', f);
        }
    fputs("];\n", f);
}
void addons_idle(const int *enabled)
{
    int j;
    DWORD v;
    HWND w;
    if (!enabled[12])
    {
        for (j = 0; j < 2; j++)
            if (diskStarted[j])
            {
                stat_read("StopStat", j, &v);
                diskStarted[j] = 0;
            }
        diskTick = 0;
    }
    if (!enabled[14])
    {
        w = FindWindowA("W98Resources16", NULL);
        if (w)
            PostMessageA(w, WM_CLOSE, 0, 0);
        resourceRetry = 0;
    }
}
void addons_collect(FILE *f, const char *root, const char *ini, const int *enabled)
{
    static DWORD lastRecent, lastMedia;
    static char cachedRecent[16384], cachedMedia[16384];
    char path[MAX_PATH];
    FILE *cache;
    size_t n;
    DWORD tick = GetTickCount();
    (void)ini;
    if (enabled[12])
        disk_activity(f);
    else
        fputs("var diskActivity={read:-1,write:-1};\n", f);
    if (enabled[14])
        resources(f, root);
    else
        fputs("var resources={system:-1,user:-1,gdi:-1};\n", f);
    if (enabled[17])
    {
        if (!lastRecent || tick - lastRecent > 10000)
        {
            sprintf(path, "%s\\RECENT.TMP", root);
            cache = fopen(path, "wb");
            if (cache)
            {
                recent(cache);
                fclose(cache);
                cache = fopen(path, "rb");
                if (cache)
                {
                    n = fread(cachedRecent, 1, sizeof(cachedRecent) - 1, cache);
                    cachedRecent[n] = 0;
                    fclose(cache);
                }
            }
            lastRecent = tick;
        }
        fputs(cachedRecent, f);
    }
    else
        lastRecent = 0;
    if (enabled[18])
    {
        if (!lastMedia || tick - lastMedia > 5000)
        {
            sprintf(path, "%s\\MEDIA.TMP", root);
            cache = fopen(path, "wb");
            if (cache)
            {
                media(cache);
                fclose(cache);
                cache = fopen(path, "rb");
                if (cache)
                {
                    n = fread(cachedMedia, 1, sizeof(cachedMedia) - 1, cache);
                    cachedMedia[n] = 0;
                    fclose(cache);
                }
            }
            lastMedia = tick;
        }
        fputs(cachedMedia, f);
    }
    else
        lastMedia = 0;
}
void addons_stop(void)
{
    int disabled[22] = {0};
    addons_idle(disabled);
}
