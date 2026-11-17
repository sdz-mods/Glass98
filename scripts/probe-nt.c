/* Ten-second XP native accounting probe; load uses one busy thread and a 5 ms timer. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/bridge/telemetry.h"
#include "../src/bridge/processes.h"
static volatile LONG running = 1;
static DWORD WINAPI load(void *unused)
{
    volatile DWORD n = 1;
    (void)unused;
    while (running)
        n = n * 1664525 + 1013904223;
    return n;
}
void js_string(FILE *file, const char *s)
{
    fputc('\'', file);
    while (*s)
    {
        if (*s == '\'' || *s == '\\')
            fputc('\\', file);
        fputc(*s++, file);
    }
    fputc('\'', file);
}
int main(int argc, char **argv)
{
    TELEMETRY state;
    HANDLE thread = NULL;
    FILE *f;
    int i, busy = argc > 1 && !strcmp(argv[1], "load");
    OSVERSIONINFOA os;
    char text[512];
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    if (os.dwPlatformId != VER_PLATFORM_WIN32_NT || os.dwMajorVersion != 5 || os.dwMinorVersion != 1)
    {
        puts("Run this probe inside 32-bit Windows XP.");
        return 2;
    }
    f = stdout;
    if (busy)
        thread = CreateThread(NULL, 0, load, NULL, 0, NULL);
    timeBeginPeriod(5);
    telemetry_init(&state);
    processes_init();
    for (i = 0; i < 10; i++)
    {
        Sleep(1000);
        telemetry_text(&state, text);
        fprintf(f, "%s\n", text);
        processes_update("", 1, 1);
        processes_write(f);
        fflush(f);
    }
    processes_pause(1);
    processes_write(f);
    processes_stop();
    telemetry_stop(&state);
    running = 0;
    if (thread)
    {
        WaitForSingleObject(thread, 5000);
        CloseHandle(thread);
    }
    timeEndPeriod(5);
    return 0;
}
