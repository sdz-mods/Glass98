/* Register the standalone CPU-counter test driver; no Glass98 settings change. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define DRIVER_KEY "System\\CurrentControlSet\\Services\\VxD\\G98CPU"

int main(int argc, char **argv)
{
    OSVERSIONINFOA version;
    char path[MAX_PATH], shortPath[MAX_PATH], root[4];
    char *name;
    DWORD length, disposition;
    HKEY key;
    LONG error;
    BYTE start = 0;

    if (argc != 2 || (strcmp(argv[1], "install") && strcmp(argv[1], "remove")))
    {
        printf("Usage: CPUSETUP install | remove\nRestart Windows after either operation.\n");
        return 1;
    }
    memset(&version, 0, sizeof(version));
    version.dwOSVersionInfoSize = sizeof(version);
    if (!GetVersionExA(&version) || version.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS ||
        version.dwMajorVersion != 4 || version.dwMinorVersion != 10)
    {
        printf("This test driver requires Windows 98.\n");
        return 1;
    }
    if (!strcmp(argv[1], "remove"))
    {
        error = RegDeleteKeyA(HKEY_LOCAL_MACHINE, DRIVER_KEY);
        if (error == ERROR_FILE_NOT_FOUND)
            error = ERROR_SUCCESS;
        if (!error)
            error = RegFlushKey(HKEY_LOCAL_MACHINE);
    }
    else
    {
        length = GetModuleFileNameA(NULL, path, sizeof(path));
        if (!length || length >= sizeof(path) || !(name = strrchr(path, '\\')) ||
            (size_t)(name + 1 - path) + sizeof("G98CPU.VXD") > sizeof(path))
            return 1;
        strcpy(name + 1, "G98CPU.VXD");
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        if (path[1] != ':' || GetDriveTypeA(root) != DRIVE_FIXED ||
            GetFileAttributesA(path) == 0xffffffffUL)
        {
            printf("Copy this complete test package to a local hard disk first.\n");
            return 1;
        }
        length = GetShortPathNameA(path, shortPath, sizeof(shortPath));
        if (!length || length >= sizeof(shortPath))
            return 1;
        error = RegCreateKeyExA(HKEY_LOCAL_MACHINE, DRIVER_KEY, 0, NULL, 0,
                                KEY_ALL_ACCESS, NULL, &key, &disposition);
        if (!error)
        {
            error = RegSetValueExA(key, "StaticVxD", 0, REG_SZ,
                                  (const BYTE *)shortPath, strlen(shortPath) + 1);
            if (!error)
                error = RegSetValueExA(key, "Start", 0, REG_BINARY, &start, sizeof(start));
            if (!error)
                error = RegFlushKey(key);
            RegCloseKey(key);
        }
        if (!error)
            printf("Registered %s\n", shortPath);
    }
    if (error)
    {
        printf("Registry operation failed: %ld\n", error);
        return 2;
    }
    printf("Restart Windows to complete %s.\n", argv[1]);
    return 0;
}
