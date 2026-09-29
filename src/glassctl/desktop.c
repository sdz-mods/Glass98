/* Local CD title editing and offline edition selection. */
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int desktop_action(const char *cmd, const char *ini, const char *root)
{
    char path[MAX_PATH], preview[MAX_PATH], key[40], tail, identifier[40], section[80], args[MAX_PATH + 4], value[512];
    int slot, tracks, i;
    (void)ini;
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
