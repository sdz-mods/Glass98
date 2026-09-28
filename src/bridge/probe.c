#include "telemetry.h"
#include <stdio.h>
#include <string.h>
static void version(const char *file)
{
    DWORD ignored, size;
    void *data;
    VS_FIXEDFILEINFO *info;
    UINT length;
    size = GetFileVersionInfoSizeA(file, &ignored);
    if (!size)
    {
        printf("%s: version unavailable\n", file);
        return;
    }
    data = GlobalAlloc(GPTR, size);
    if (!data)
        return;
    if (GetFileVersionInfoA(file, 0, size, data) && VerQueryValueA(data, "\\", (void**)&info, &length))
        printf("%s: %u.%u.%u.%u\n", file, HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
               HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    GlobalFree(data);
}
int main(int argc, char **argv)
{
    TELEMETRY state;
    char text[512];
    int i;
    DWORD end;
    volatile DWORD work = 1;
    int load = argc > 1 && !strcmp(argv[1], "load");
    void *allocation = NULL;
    if (argc > 1 && !strcmp(argv[1], "memory"))
    {
        allocation = VirtualAlloc(NULL, 32UL * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
        if (!allocation)
            return 5;
        memset(allocation, 0x5a, 32UL * 1024 * 1024);
        puts("Holding 32 MiB committed and touched memory for 12 seconds.");
    }
    if (argc > 1 && !strcmp(argv[1], "info"))
    {
        OSVERSIONINFOA os;
        HKEY key;
        char value[256];
        DWORD size, type;
        memset(&os, 0, sizeof(os));
        os.dwOSVersionInfoSize = sizeof(os);
        GetVersionExA(&os);
        printf("Windows %lu.%lu build %lu platform %lu %s\n", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber & 65535,
               os.dwPlatformId, os.szCSDVersion);
        version("C:\\WINDOWS\\SYSTEM\\SHELL32.DLL");
        version("C:\\WINDOWS\\SYSTEM\\SHDOCVW.DLL");
        version("C:\\WINDOWS\\SYSTEM\\MSHTML.DLL");
        version("C:\\WINDOWS\\SYSTEM\\JSCRIPT.DLL");
        if (!RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &key))
        {
            size = sizeof(value);
            if (!RegQueryValueExA(key, "W98WidgetsTelemetry", NULL, &type, (BYTE*)value, &size))
            {
                value[255] = 0;
                printf("Widget startup: %s\n", value);
            }
            else
                puts("Widget startup: absent");
            RegCloseKey(key);
        }
        if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\PerfStats\\Enum\\KERNEL\\CPUUsage", 0,
                           KEY_READ, &key))
        {
            size = sizeof(value);
            if (!RegQueryValueExA(key, "Differentiate", NULL, &type, (BYTE*)value, &size))
            {
                value[255] = 0;
                printf("CPU counter Differentiate=%s type=%lu\n", value, type);
            }
            RegCloseKey(key);
        }
        return 0;
    }
    telemetry_init(&state);
    printf("Start: success=%d error=%ld\n", state.started, state.lastError);
    for (i = 0; i < 12; i++)
    {
        if (load)
        {
            end = GetTickCount() + 1000;
            while ((LONG)(end - GetTickCount()) > 0)
                work = work * 1664525 + 1013904223;
        }
        else
            Sleep(1000);
        telemetry_text(&state, text);
        printf("%d type=%lu size=%lu error=%ld %s\n", i, state.counterType, state.counterSize, state.lastError, text);
    }
    telemetry_stop(&state);
    if (allocation)
        VirtualFree(allocation, 0, MEM_RELEASE);
    return 0;
}
