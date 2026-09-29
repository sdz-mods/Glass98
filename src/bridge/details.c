/* Demand-driven memory, desktop state and plain-text clipboard snapshots. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "cdcatalog.h"
#include "extras.h"
#include "../widgets.h"
#include "../winschemes.h"

static double largest_block(void)
{
    SYSTEM_INFO system;
    MEMORY_BASIC_INFORMATION region;
    DWORD address, next, largest = 0, started = GetTickCount();
    unsigned int count = 0;
    GetSystemInfo(&system);
    address = (DWORD)system.lpMinimumApplicationAddress;
    while (address < (DWORD)system.lpMaximumApplicationAddress)
    {
        if (!VirtualQuery((void*)address, &region, sizeof(region)))
            return -1;
        if (region.State == MEM_FREE && region.RegionSize > largest)
            largest = region.RegionSize;
        next = (DWORD)region.BaseAddress + region.RegionSize;
        if (next <= address)
            break;
        address = next;
        if (++count > 8192 || GetTickCount() - started > 100)
            return -1;
    }
    return (double)largest;
}

static void desktop_schemes(FILE *f, int enabled)
{
    static char names[128][SCHEME_NAME], current[SCHEME_NAME];
    static DWORD sampled;
    static int count;
    HMODULE library;
    HKEY key;
    DWORD now = GetTickCount(), index, length, type, size;
    int i, j;
    char name[SCHEME_NAME];
    fputs("var windowsSchemes=[", f);
    if (!enabled)
    {
        sampled = 0;
        fputs("];var windowsSchemeCurrent='';\n", f);
        return;
    }
    if (!sampled || now - sampled >= 30000)
    {
        sampled = now;
        count = 0;
        library = windows_scheme_library();
        if (library && GetProcAddress(library, "DeskSetCurrentSchemeA") &&
                !RegOpenKeyA(HKEY_CURRENT_USER, SCHEME_KEY, &key))
        {
            for (index = 0; index < 1024 && count < 128; index++)
            {
                length = sizeof(name);
                size = 0;
                if (RegEnumValueA(key, index, name, &length, NULL, &type, NULL, &size))
                    break;
                if (type == REG_BINARY && size && length && length < sizeof(name))
                    lstrcpynA(names[count++], name, SCHEME_NAME);
            }
            RegCloseKey(key);
        }
        if (library)
            FreeLibrary(library);
    }
    current[0] = 0;
    if (!RegOpenKeyA(HKEY_CURRENT_USER, "Control Panel\\Appearance", &key))
    {
        size = sizeof(current);
        if (RegQueryValueExA(key, "Current", NULL, &type, (BYTE*)current, &size) || type != REG_SZ)
            current[0] = 0;
        current[sizeof(current) - 1] = 0;
        RegCloseKey(key);
    }
    for (i = 0; i < count; i++)
    {
        if (i)
            fputc(',', f);
        fputc('[', f);
        fputc('\'', f);
        for (j = 0; names[i][j]; j++)
            fprintf(f, "%02X", (unsigned char)names[i][j]);
        fputs("',", f);
        js_string(f, names[i]);
        fputc(']', f);
    }
    fputs("];var windowsSchemeCurrent=", f);
    fputc('\'', f);
    for (j = 0; current[j]; j++)
        fprintf(f, "%02X", (unsigned char)current[j]);
    fputs("';\n", f);
}

void details_collect(FILE *f, const char *ini, int *enabled, int seconds)
{
    static DWORD sampled;
    static MEMORYSTATUS memory;
    static double largest = -1;
    BOOL saver = FALSE;
    char text[2049];
    HANDLE handle;
    const char *data;
    DWORD size, count;
    int state = 0, clipped = 0;
    (void)ini;
    if (enabled[WIDGET_MEMORY])
    {
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatus(&memory);
        if (!sampled || GetTickCount() - sampled >= (DWORD)(seconds < 10 ? 10 : seconds) * 1000)
        {
            largest = largest_block();
            sampled = GetTickCount();
        }
        fprintf(f,
                "var memoryDetails={physical:%.0f,available:%.0f,pageTotal:%.0f,pageAvailable:%.0f,virtualTotal:%.0f,virtualAvailable:%.0f,largest:%.0f};\n",
                (double)memory.dwTotalPhys, (double)memory.dwAvailPhys,
                (double)memory.dwTotalPageFile, (double)memory.dwAvailPageFile,
                (double)memory.dwTotalVirtual, (double)memory.dwAvailVirtual, largest);
    }
    else
    {
        sampled = 0;
        fputs("var memoryDetails={};\n", f);
    }
    if (enabled[WIDGET_DESKTOP])
        SystemParametersInfoA(SPI_GETSCREENSAVEACTIVE, 0, &saver, 0);
    fprintf(f, "var desktopState={saver:%d};\n", saver != FALSE);
    desktop_schemes(f, enabled[WIDGET_DESKTOP]);
    text[0] = 0;
    if (enabled[WIDGET_CLIPBOARD])
    {
        state = -1;
        if (OpenClipboard(NULL))
        {
            state = IsClipboardFormatAvailable(CF_TEXT) ? 1 : 0;
            handle = state == 1 ? GetClipboardData(CF_TEXT) : NULL;
            if (handle && (data = GlobalLock(handle)) != NULL)
            {
                size = GlobalSize(handle);
                for (count = 0; count < size && count < sizeof(text) - 1 && data[count]; count++)
                    text[count] = data[count];
                text[count] = 0;
                clipped = count == sizeof(text) - 1 && count < size && data[count] != 0;
                GlobalUnlock(handle);
            }
            else if (state == 1)
                state = -1;
            CloseClipboard();
        }
    }
    fputs("var clipboardData={text:", f);
    js_string(f, text);
    fprintf(f, ",state:%d,truncated:%d};\n", state, clipped);
}


static int cd_frames(const char *command, unsigned long *frames)
{
    char text[80], tail;
    unsigned int m, s, f;
    if (mciSendStringA(command, text, sizeof(text), NULL) ||
            sscanf(text, "%u:%u:%u%c", &m, &s, &f, &tail) != 3 || m > 200 || s > 59 || f > 74)
        return 0;
    *frames = (m * 60UL + s) * 75 + f;
    return 1;
}

static int cd_layout(int tracks, unsigned long *offsets, unsigned long *duration)
{
    char command[80];
    MCI_STATUS_PARMS status;
    MCIDEVICEID device = mciGetDeviceIDA("w98cd");
    unsigned long length, start;
    int i, ok = 0;
    if (!device || mciSendStringA("set w98cd time format msf", NULL, 0, NULL))
        return 0;
    for (i = 0; i < tracks; i++)
    {
        /* String track types are localized; use the numeric MCI result. */
        memset(&status, 0, sizeof(status));
        status.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
        status.dwTrack = i + 1;
        if (mciSendCommandA(device, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR)&status) ||
                status.dwReturn != MCI_CDA_TRACK_AUDIO)
            goto done;
        sprintf(command, "status w98cd position track %d", i + 1);
        if (!cd_frames(command, &offsets[i]))
            goto done;
    }
    sprintf(command, "status w98cd length track %d", tracks);
    if (!cd_frames(command, &length))
        goto done;
    start = offsets[0];
    *duration = offsets[tracks - 1] + length - start;
    for (i = 0; i < tracks; i++)
        offsets[i] -= start;
    ok = 1;
done:
    /* Playback uses track-based addressing; restore it even after a query failure. */
    if (mciSendStringA("set w98cd time format tmsf", NULL, 0, NULL))
        ok = 0;
    return ok;
}

static void cd_ansi(const char *utf8, char *out, int size)
{
    WCHAR wide[CD_CATALOG_TEXT];
    out[0] = 0;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, CD_CATALOG_TEXT))
    {
        /* Keep complete characters when the current Windows code page expands them. */
        while (!WideCharToMultiByte(CP_ACP, 0, wide, -1, out, size, NULL, NULL))
        {
            int length = lstrlenW(wide);
            if (!length)
                break;
            wide[length - 1] = 0;
        }
    }
    out[size - 1] = 0;
}

static void cd_text(FILE *f, const char *text)
{
    WCHAR wide[512];
    int i;
    if (!MultiByteToWideChar(CP_ACP, 0, text, -1, wide, 512))
    {
        js_string(f, text);
        return;
    }
    fputc('\'', f);
    for (i = 0; wide[i]; i++)
    {
        if (wide[i] < 32 || wide[i] >= 127 || wide[i] == '\'' || wide[i] == '\\' || wide[i] == '<')
            fprintf(f, "\\u%04x", (unsigned int)wide[i]);
        else
            fputc((char)wide[i], f);
    }
    fputc('\'', f);
}

void details_cd(FILE *f, const char *root, int opened, int track, int tracks)
{
    static char previous[80] = "";
    static CD_CATALOG_RESULT cached;
    static int matches, lastChoice = -1;
    char identity[64] = "", section[80] = "", db[MAX_PATH], catalog[MAX_PATH], temp[MAX_PATH];
    char album[512] = "", artist[512] = "", title[512] = "", fallback[512], key[24];
    unsigned long offsets[99], duration;
    unsigned int i;
    int choice = 0, installed;
    FILE *preview;
    sprintf(db, "%s\\CDTITLES.INI", root);
    sprintf(catalog, "%s\\CDMETA.DAT", root);
    sprintf(temp, "%s\\CDLOOKUP.TMP", root);
    installed = GetFileAttributesA(catalog) != INVALID_FILE_ATTRIBUTES;
    if (opened && tracks > 0 && tracks <= 99 &&
            !mciSendStringA("info w98cd identity", identity, sizeof(identity), NULL))
    {
        for (i = 0; identity[i]; i++)
            if (!isalnum((unsigned char)identity[i]) && identity[i] != '-')
                break;
        if (identity[i] || i > 32)
            identity[0] = 0;
    }
    if (identity[0])
    {
        sprintf(section, "CD_%s_%d", identity, tracks);
        choice = GetPrivateProfileIntA(section, "DatabaseChoice", 0, db);
        if (choice < 0 || choice > 255)
            choice = 0;
        if (strcmp(previous, section) || choice != lastChoice)
        {
            strcpy(previous, section);
            lastChoice = choice;
            memset(&cached, 0, sizeof(cached));
            matches = 0;
            if (installed && cd_layout(tracks, offsets, &duration))
                matches = cd_catalog_lookup(catalog, offsets, tracks, duration, choice, &cached);
            DeleteFileA(temp);
            if (matches > 0 && (preview = fopen(temp, "wb")) != NULL)
            {
                fprintf(preview, "[%s]\r\n", section);
                cd_ansi(cached.album, fallback, sizeof(fallback));
                fprintf(preview, "Album=%s\r\n", fallback);
                cd_ansi(cached.artist, fallback, sizeof(fallback));
                fprintf(preview, "Artist=%s\r\n", fallback);
                for (i = 0; i < (unsigned int)tracks; i++)
                {
                    cd_ansi(cached.tracks[i], fallback, sizeof(fallback));
                    fprintf(preview, "Track%u=%s\r\n", i + 1, fallback);
                }
                fclose(preview);
            }
        }
        cd_ansi(cached.album, fallback, sizeof(fallback));
        GetPrivateProfileStringA(section, "Album", fallback, album, sizeof(album), db);
        cd_ansi(cached.artist, fallback, sizeof(fallback));
        GetPrivateProfileStringA(section, "Artist", fallback, artist, sizeof(artist), db);
        fallback[0] = 0;
        if (track > 0 && track <= tracks)
            cd_ansi(cached.tracks[track - 1], fallback, sizeof(fallback));
        sprintf(key, "Track%d", track);
        GetPrivateProfileStringA(section, key, fallback, title, sizeof(title), db);
    }
    else
    {
        previous[0] = 0;
        matches = 0;
    }
    fputs("var cdInfo={id:", f);
    js_string(f, identity);
    fputs(",album:", f);
    cd_text(f, album);
    fputs(",artist:", f);
    cd_text(f, artist);
    fputs(",title:", f);
    cd_text(f, title);
    fprintf(f, ",matches:%d,choice:%d,catalog:%d};\n", matches, matches > 0 && choice < matches ? choice : 0, installed);
}
