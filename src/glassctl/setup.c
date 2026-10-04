/* Win98 package installer. Runs from TEMP so installed executables can be replaced. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef TARGET
#define TARGET "C:\\Glass98"
#endif
#define CPU_DRIVER_KEY "System\\CurrentControlSet\\Services\\VxD\\G98CPU"
static int driverRestart;
static const char *files[] = {
    "G98SETUP.EXE", "GADGETCTL.EXE", "W98DATA.EXE", "GLASSCTL.EXE", "GLASSPRF.EXE", "RSRC16.EXE",
    "GLASS.HTM", "SETTINGS.HTM", "WALL.BMP", "WIDGETS.JS", "ADDONS.JS", "UTILITIES.JS", "THEMES.JS",
    "MANAGER.JS", "MANAGER.CSS", "PLACEMENT.JS", "DATA.JS", "RUNSTATE.JS", "EXTRA.JS", "RSS.JS",
    "PING.JS", "ACK.JS", "VERSION.TXT", "INSTALL.BAT", "REMOVE.BAT", "README.TXT", "LICENSE.TXT",
    "NOTICE.TXT", "WATCOM.TXT", "VMDISP9X.TXT", "G98CPU.VXD", NULL
};
static int exists(const char *p)
{
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static int choose_cpu(void)
{
    char previous[16];
    UINT flags = MB_YESNOCANCEL | MB_ICONQUESTION;
    int answer;
    GetPrivateProfileStringA("CPU", "Method", "VxD", previous, sizeof(previous), TARGET "\\WIDGETS.INI");
    if (!lstrcmpiA(previous, "Legacy"))
        flags |= MB_DEFBUTTON2;
    answer = MessageBoxA(NULL,
        "Use the recommended VxD CPU meter?\n\n"
        "Yes: install the Glass98 CPU driver for better CPU readings when\n"
        "programs change Windows' timer resolution.\n"
        "Restart required; CPU usage is unavailable until the driver loads.\n\n"
        "No: use the original Windows CPU counter without a driver.\n"
        "Its readings can be incorrect when the timer resolution changes.\n\n"
        "Run INSTALL.BAT again to change this choice.",
        "Glass98 - CPU measurement", flags);
    return answer == IDCANCEL ? -1 : answer == IDYES;
}

static int configure_cpu_driver(int enable)
{
    HKEY key;
    LONG error;
    DWORD disposition;
    BYTE start = 0;
    if (!enable)
    {
        error = RegDeleteKeyA(HKEY_LOCAL_MACHINE, CPU_DRIVER_KEY);
        if (error == ERROR_FILE_NOT_FOUND)
            return 1;
        if (!error)
        {
            driverRestart = 1;
            error = RegFlushKey(HKEY_LOCAL_MACHINE);
        }
    }
    else
    {
        error = RegCreateKeyExA(HKEY_LOCAL_MACHINE, CPU_DRIVER_KEY, 0, NULL, 0,
                               KEY_ALL_ACCESS, NULL, &key, &disposition);
        if (!error)
        {
            error = RegSetValueExA(key, "StaticVxD", 0, REG_SZ,
                                  (const BYTE *)TARGET "\\G98CPU.VXD", sizeof(TARGET "\\G98CPU.VXD"));
            if (!error)
                error = RegSetValueExA(key, "Start", 0, REG_BINARY, &start, sizeof(start));
            if (!error)
                error = RegFlushKey(key);
            RegCloseKey(key);
            driverRestart = 1;
        }
    }
    if (error)
        printf("Could not configure CPU driver (error %ld).\n", error);
    return error == ERROR_SUCCESS;
}

static void restart_notice(void)
{
    if (driverRestart)
    {
        puts("Restart Windows to finish the CPU driver change.");
        MessageBoxA(NULL, "Restart Windows to finish the CPU driver change.\n\n"
                    "Save your work and restart when ready.", "Glass98", MB_OK | MB_ICONINFORMATION);
    }
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
static int choose_catalog(const char *source)
{
    char path[MAX_PATH], message[512], magic[8];
    long size;
    FILE *file;
    int answer;
    if (strlen(source) + 15 >= MAX_PATH)
        return -1;
    sprintf(path, "%s\\CDMETA.DAT", source);
    if (!exists(path))
        return 0;
    file = fopen(path, "rb");
    if (!file)
        return -1;
    if (fread(magic, 1, 8, file) != 8 || memcmp(magic, "G98CDDB1", 8) ||
            fseek(file, 0, SEEK_END) || (size = ftell(file)) < 32)
    {
        fclose(file);
        puts("The optional CD database is invalid.");
        return -1;
    }
    fclose(file);
    sprintf(message, "Install the optional offline CD title database?\n\n"
            "MusicBrainz album and track names, with no Internet connection required.\n"
            "Disk space: %ld MB.\n\nNo keeps any database already installed.", size / 1048576L + (size % 1048576L != 0));
    answer = MessageBoxA(NULL, message, "Glass98 - Optional CD database", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
    return answer == IDCANCEL ? -1 : answer == IDYES;
}

static int install_catalog(const char *source)
{
    char from[MAX_PATH], to[MAX_PATH];
    const char *notices[] = {"CDDATA.TXT", "CC0.TXT", NULL};
    int i, previous = exists(TARGET "\\CDMETA.DAT");
    if (!lstrcmpiA(source, TARGET))
        return 1;
    for (i = 0; notices[i]; i++)
    {
        sprintf(from, "%s\\%s", source, notices[i]);
        sprintf(to, TARGET "\\%s", notices[i]);
        if (exists(to))
            SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
        if (!CopyFileA(from, to, FALSE))
            return 0;
        SetFileAttributesA(to, FILE_ATTRIBUTE_NORMAL);
    }
    sprintf(from, "%s\\CDMETA.DAT", source);
    if (!CopyFileA(from, TARGET "\\CDMETA.NEW", FALSE))
    {
        DeleteFileA(TARGET "\\CDMETA.NEW");
        return 0;
    }
    SetFileAttributesA(TARGET "\\CDMETA.NEW", FILE_ATTRIBUTE_NORMAL);
    /* Finish copying before replacing an existing catalog; preserve it on failure. */
    if (previous)
    {
        SetFileAttributesA(TARGET "\\CDMETA.BAK", FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(TARGET "\\CDMETA.BAK");
        if (!MoveFileA(TARGET "\\CDMETA.DAT", TARGET "\\CDMETA.BAK"))
            return 0;
    }
    if (!MoveFileA(TARGET "\\CDMETA.NEW", TARGET "\\CDMETA.DAT"))
    {
        if (previous)
            MoveFileA(TARGET "\\CDMETA.BAK", TARGET "\\CDMETA.DAT");
        return 0;
    }
    SetFileAttributesA(TARGET "\\CDMETA.BAK", FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(TARGET "\\CDMETA.BAK");
    return 1;
}

int main(int argc, char **argv)
{
    char source[MAX_PATH], temp[MAX_PATH], stage[MAX_PATH], from[MAX_PATH], to[MAX_PATH], *raw, *p;
    HANDLE lock;
    OSVERSIONINFOA os;
    DWORD length;
    int i, install, catalog = 0, cpuVxd = 0, result = 1, staged = 0;
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
        if (!configure_cpu_driver(0))
            goto done;
        if (!remove_files())
            goto done;
        puts("Glass98 removed. REMOVE.BAT and all INI files retained in " TARGET ".");
        result = 0;
        goto done;
    }
    cpuVxd = choose_cpu();
    if (cpuVxd < 0)
    {
        puts("Installation cancelled. No changes made.");
        result = 0;
        goto done;
    }
    catalog = choose_catalog(source);
    if (catalog < 0)
        goto done;
    length = GetTempPathA(sizeof(temp), temp);
    if (!length || length > MAX_PATH - 30 || !GetTempFileNameA(temp, "G98", 0, stage))
        goto done;
    DeleteFileA(stage);
    if (!CreateDirectoryA(stage, NULL))
        goto done;
    staged = 1;
    for (i = 0; files[i]; i++)
    {
        if (!cpuVxd && !lstrcmpiA(files[i], "G98CPU.VXD"))
            continue;
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
        if (!cpuVxd && !lstrcmpiA(files[i], "G98CPU.VXD"))
            continue;
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
    if (!configure_cpu_driver(cpuVxd))
        goto done;
    if (!WritePrivateProfileStringA("CPU", "Method", cpuVxd ? "VxD" : "Legacy", TARGET "\\WIDGETS.INI"))
    {
        puts("Could not save the CPU measurement method.");
        goto done;
    }
    WritePrivateProfileStringA(NULL, NULL, NULL, TARGET "\\WIDGETS.INI");
    if (!cpuVxd && exists(TARGET "\\G98CPU.VXD") && !DeleteFileA(TARGET "\\G98CPU.VXD"))
        puts("The unused G98CPU.VXD file can be deleted after restarting Windows.");
    if (catalog && !install_catalog(source))
    {
        puts("Could not install the optional CD database. Check free disk space and retry, or choose No.");
        goto done;
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
    restart_notice();
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
