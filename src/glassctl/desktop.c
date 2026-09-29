/* Explicit desktop actions; paths come from file dialogs and saved settings. */
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../winschemes.h"

static int select_file(const char *ini, const char *key)
{
    OPENFILENAMEA dialog;
    char path[MAX_PATH] = "";
    memset(&dialog, 0, sizeof(dialog));
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path);
    dialog.lpstrTitle = "Choose a wallpaper";
    dialog.lpstrFilter = "Images (*.bmp;*.jpg;*.jpeg;*.gif)\0*.bmp;*.jpg;*.jpeg;*.gif\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    return !GetOpenFileNameA(&dialog) || WritePrivateProfileStringA("Options", key, path, ini);
}

static int copy_character(unsigned int code)
{
    WCHAR character = (WCHAR)code;
    char text[8];
    int length;
    BOOL substituted = FALSE;
    HGLOBAL memory;
    HWND owner;
    void *data;
    if (code < 32 || code > 65535 || (code >= 0xD800 && code <= 0xDFFF))
        return 0;
    length = WideCharToMultiByte(CP_ACP, 0, &character, 1, text, 7, NULL, &substituted);
    if (!length || substituted)
        return 0;
    text[length] = 0;
    memory = GlobalAlloc(GMEM_MOVEABLE, length + 1);
    if (!memory)
        return 0;
    data = GlobalLock(memory);
    if (!data)
    {
        GlobalFree(memory);
        return 0;
    }
    memcpy(data, text, length + 1);
    GlobalUnlock(memory);
    owner = CreateWindowA("STATIC", "Glass98 clipboard", WS_POPUP, 0, 0, 0, 0, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!owner || !OpenClipboard(owner))
    {
        if (owner)
            DestroyWindow(owner);
        GlobalFree(memory);
        return 0;
    }
    if (!EmptyClipboard() || !SetClipboardData(CF_TEXT, memory))
    {
        CloseClipboard();
        DestroyWindow(owner);
        GlobalFree(memory);
        return 0;
    }
    CloseClipboard();
    DestroyWindow(owner);
    return 1;
}

int desktop_action(const char *cmd, const char *ini, const char *root)
{
    char path[MAX_PATH], preview[MAX_PATH], key[40], tail, identifier[40], section[80], args[MAX_PATH + 4], value[512];
    int slot, tracks, i;
    unsigned int code;
    if (!strncmp(cmd, "scheme/", 7))
        return windows_scheme_apply(cmd + 7);
    if (sscanf(cmd, "wallchoose/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "wall%d", slot);
        return select_file(ini, key);
    }
    if (sscanf(cmd, "wallclear/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "wall%d", slot);
        return WritePrivateProfileStringA("Options", key, "", ini);
    }
    if (sscanf(cmd, "wallset/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "wall%d", slot);
        GetPrivateProfileStringA("Options", key, "", path, sizeof(path), ini);
        if (!*path || GetFileAttributesA(path) == 0xFFFFFFFFUL)
            return 0;
        return WritePrivateProfileStringA("Desktop", "Wallpaper", path, ini);
    }
    if (!strcmp(cmd, "display"))
        return (UINT)ShellExecuteA(NULL, "open", "CONTROL.EXE", "DESK.CPL,,2", NULL, SW_SHOWNORMAL) > 32;
    if (!strcmp(cmd, "saversettings"))
        return (UINT)ShellExecuteA(NULL, "open", "CONTROL.EXE", "DESK.CPL,,1", NULL, SW_SHOWNORMAL) > 32;
    if (!strcmp(cmd, "saverstart"))
        return PostMessageA(GetDesktopWindow(), WM_SYSCOMMAND, SC_SCREENSAVE, 0);
    if (sscanf(cmd, "saver/%d%c", &slot, &tail) == 1 && (slot == 0 || slot == 1))
        return SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, slot, NULL, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    if (sscanf(cmd, "character/%u%c", &code, &tail) == 1)
        return copy_character(code);
    if (sscanf(cmd, "cdchoice/%32[^/]/%d/%d%c", identifier, &tracks, &slot, &tail) == 3 &&
            tracks > 0 && tracks <= 99 && slot >= 0 && slot < 256)
    {
        for (i = 0; identifier[i]; i++)
            if (!isalnum((unsigned char)identifier[i]) && identifier[i] != '-')
                return 0;
        sprintf(path, "%s\\CDTITLES.INI", root);
        sprintf(section, "CD_%s_%d", identifier, tracks);
        sprintf(value, "%d", slot);
        return WritePrivateProfileStringA(section, "DatabaseChoice", value, path);
    }
    if (sscanf(cmd, "cdedit/%32[^/]/%d%c", identifier, &tracks, &tail) == 2 && tracks > 0 && tracks <= 99)
    {
        for (i = 0; identifier[i]; i++)
            if (!isalnum((unsigned char)identifier[i]) && identifier[i] != '-')
                return 0;
        sprintf(path, "%s\\CDTITLES.INI", root);
        sprintf(preview, "%s\\CDLOOKUP.TMP", root);
        sprintf(section, "CD_%s_%d", identifier, tracks);
        GetPrivateProfileStringA(section, "Album", "", value, sizeof(value), path);
        if (!*value)
        {
            GetPrivateProfileStringA(section, "Album", "Album title", value, sizeof(value), preview);
            WritePrivateProfileStringA(section, "Album", value, path);
        }
        GetPrivateProfileStringA(section, "Artist", "", value, sizeof(value), path);
        if (!*value)
        {
            GetPrivateProfileStringA(section, "Artist", "Artist name", value, sizeof(value), preview);
            WritePrivateProfileStringA(section, "Artist", value, path);
        }
        for (i = 1; i <= tracks; i++)
        {
            sprintf(key, "Track%d", i);
            GetPrivateProfileStringA(section, key, "", value, sizeof(value), path);
            if (!*value)
            {
                sprintf(value, "Track %d", i);
                strcpy(args, value);
                GetPrivateProfileStringA(section, key, args, value, sizeof(value), preview);
                WritePrivateProfileStringA(section, key, value, path);
            }
        }
        WritePrivateProfileStringA(NULL, NULL, NULL, path);
        sprintf(args, "\"%s\"", path);
        return (UINT)ShellExecuteA(NULL, "open", "NOTEPAD.EXE", args, root, SW_SHOWNORMAL) > 32;
    }
    return -1;
}
