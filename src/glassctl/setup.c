/* Win98 package installer. Runs from TEMP so installed executables can be replaced. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define TARGET "C:\\Glass98"
static const char *files[] = {"G98SETUP.EXE", "GADGETCTL.EXE", "W98DATA.EXE", "GLASSCTL.EXE", "GLASSPRF.EXE", "RSRC16.EXE", "GLASS.HTM", "SETTINGS.HTM", "WALL.BMP", "WIDGETS.JS", "ADDONS.JS", "THEMES.JS", "MANAGER.JS", "MANAGER.CSS", "PLACEMENT.JS", "DATA.JS", "EXTRA.JS", "RSS.JS", "PING.JS", "ACK.JS", "VERSION.TXT", "INSTALL.BAT", "REMOVE.BAT", "README.TXT", "LICENSE.TXT", "NOTICE.TXT", "WATCOM.TXT", NULL};
static int exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static int run(const char *dir, const char *file, const char *args)
{
    char exe[MAX_PATH], cmd[1024];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code;
    sprintf(exe, "%s\\%s", dir, file);
    sprintf(cmd, "\"%s\" %s", exe, args);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessA(exe, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi))
    {
        printf("Cannot start %s (error %lu).\n", file, GetLastError());
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code)
        printf("%s failed (exit %lu). Installation files retained for recovery.\n", file, code);
    return code == 0;
}
static BOOL CALLBACK close_manager(HWND w, LPARAM ignored)
{
    char title[256];
    (void)ignored;
    GetWindowTextA(w, title, sizeof(title));
    if (!strncmp(title, "Glass98 - Settings", 18) || !strncmp(title, "W98 Widgets - Settings", 22))
        PostMessageA(w, WM_CLOSE, 0, 0);
    return TRUE;
}
static int stop(void)
{
    int i;
    HWND w;
    EnumWindows(close_manager, 0);
    if (exists(TARGET "\\GLASSCTL.EXE") && !run(TARGET, "GLASSCTL.EXE", "remove \"" TARGET "\\GLASS.HTM\""))
        return 0;
    if (exists(TARGET "\\GADGETCTL.EXE") && !run(TARGET, "GADGETCTL.EXE", "unregister \"" TARGET "\\W98DATA.EXE\""))
        return 0;
    if (exists(TARGET "\\GLASSPRF.EXE") && !run(TARGET, "GLASSPRF.EXE", "unregister"))
        return 0;
    w = FindWindowA("W98Resources16", NULL);
    if (w)
        PostMessageA(w, WM_CLOSE, 0, 0);
    for (i = 0; i < 50 && FindWindowA("W98Resources16", NULL); i++)
        Sleep(100);
    return !FindWindowA("W98Resources16", NULL);
}
static int remove_files(void)
{
    WIN32_FIND_DATAA data;
    HANDLE find;
    char path[MAX_PATH];
    const char *ext;
    int ok = 1;
    /* Fixed absolute target only; preserve INI files and REMOVE.BAT without recursion. */
    find = FindFirstFileA(TARGET "\\*.*", &data);
    if (find == INVALID_HANDLE_VALUE)
        return 1;
    do
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        ext = strrchr(data.cFileName, '.');
        if (!lstrcmpiA(data.cFileName, "REMOVE.BAT") || !lstrcmpiA(data.cFileName, "G98SETUP.EXE") || (ext &&
                !lstrcmpiA(ext, ".INI")))
            continue;
        sprintf(path, TARGET "\\%s", data.cFileName);
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        if (!DeleteFileA(path))
        {
            printf("Could not remove %s (error %lu).\n", path, GetLastError());
            ok = 0;
        }
    }
    while (FindNextFileA(find, &data));
    FindClose(find);
    /* Keep the remover helper available for retry if any other file was locked. */
    if (ok && exists(TARGET "\\G98SETUP.EXE"))
    {
        SetFileAttributesA(TARGET "\\G98SETUP.EXE", FILE_ATTRIBUTE_NORMAL);
        ok = DeleteFileA(TARGET "\\G98SETUP.EXE") != 0;
    }
    return ok;
}
int main(int argc, char **argv)
{
    char source[MAX_PATH], temp[MAX_PATH], stage[MAX_PATH], from[MAX_PATH], to[MAX_PATH], *raw, *p;
    HANDLE lock;
    OSVERSIONINFOA os;
    DWORD length;
    int i, install, result = 1, staged = 0;
    (void)argc;
    (void)argv;
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    if (os.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS || os.dwMinorVersion != 10 || (os.dwBuildNumber & 65535) != 2222)
    {
        puts("Glass98 setup requires Windows 98 SE.");
        return 2;
    }
    /* COMMAND.COM retains quotes in %0. Strip quotes only, preserving spaces. */
    raw = GetCommandLineA();
    if (*raw == '"')
    {
        raw = strchr(raw + 1, '"');
        if (!raw)
            return 2;
        raw++;
    }
    else
        while (*raw && *raw != ' ')
            raw++;
    while (*raw == ' ')
        raw++;
    install = !strncmp(raw, "install ", 8);
    if (!install && strncmp(raw, "remove ", 7))
        return 2;
    raw += install ? 8 : 7;
    for (i = 0; *raw && i < MAX_PATH - 1; raw++)
        if (*raw != '"')
            source[i++] = *raw;
    source[i] = 0;
    if (*raw)
        return 2;
    length = GetFullPathNameA(source, MAX_PATH, from, NULL);
    if (!length || length >= MAX_PATH)
        return 2;
    p = strrchr(from, '\\');
    if (!p)
        return 2;
    *p = 0;
    strcpy(source, from);
    lock = CreateMutexA(NULL, FALSE, "Glass98Setup");
    if (!lock)
        return 2;
    if (WaitForSingleObject(lock, 0) != WAIT_OBJECT_0)
    {
        CloseHandle(lock);
        puts("Another setup is running.");
        return 2;
    }
    if (!install)
    {
        if (!stop())
            goto done;
        if (!remove_files())
            goto done;
        puts("Glass98 removed. REMOVE.BAT and all INI files retained in " TARGET ".");
        result = 0;
        goto done;
    }
    length = GetTempPathA(sizeof(temp), temp);
    if (!length || length > MAX_PATH - 30 || !GetTempFileNameA(temp, "G98", 0, stage))
        goto done;
    DeleteFileA(stage);
    if (!CreateDirectoryA(stage, NULL))
        goto done;
    staged = 1;
    for (i = 0; files[i]; i++)
    {
        if (strlen(source) + strlen(files[i]) + 2 >= MAX_PATH)
            goto done;
        sprintf(from, "%s\\%s", source, files[i]);
        sprintf(to, "%s\\%s", stage, files[i]);
        if (!CopyFileA(from, to, FALSE))
        {
            printf("Package incomplete or unreadable: %s (error %lu).\n", from, GetLastError());
            goto done;
        }
        SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
    }
    if (!run(stage, "GLASSCTL.EXE", "check GLASS.HTM"))
        goto done;
    if (!CreateDirectoryA(TARGET, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        goto done;
    if (exists(TARGET "\\WIDGETS.INI") && !CopyFileA(TARGET "\\WIDGETS.INI", TARGET "\\WIDGETS.BAK.INI", FALSE))
    {
        puts("Could not back up WIDGETS.INI.");
        goto done;
    }
    if (!stop())
        goto done;
    for (i = 0; files[i]; i++)
    {
        sprintf(from, "%s\\%s", stage, files[i]);
        sprintf(to, TARGET "\\%s", files[i]);
        if (exists(to))
            SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
        if (!CopyFileA(from, to, FALSE))
        {
            printf("Could not update %s (error %lu).\n", to, GetLastError());
            goto done;
        }
        SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
    }
    if (!run(TARGET, "GLASSCTL.EXE", "seed \"" TARGET "\\GLASS.HTM\""))
        goto done;
    if (!run(TARGET, "GLASSPRF.EXE", "prepare"))
        goto done;
    if (!run(TARGET, "GLASSPRF.EXE", "register"))
        goto done;
    if (!run(TARGET, "GADGETCTL.EXE", "register \"" TARGET "\\W98DATA.EXE\""))
        goto done;
    if (!run(TARGET, "GLASSCTL.EXE", "install \"" TARGET "\\GLASS.HTM\""))
        goto done;
    puts("Glass98 installed in " TARGET ". Use Manage widgets on the desktop.");
    result = 0;
done:
    if (staged)
    {
        for (i = 0; files[i]; i++)
        {
            sprintf(to, "%s\\%s", stage, files[i]);
            DeleteFileA(to);
        }
        RemoveDirectoryA(stage);
    }
    if (result)
        puts("Setup did not complete. Settings are retained. Correct the reported problem and retry INSTALL.BAT or REMOVE.BAT.");
    ReleaseMutex(lock);
    CloseHandle(lock);
    return result;
}
