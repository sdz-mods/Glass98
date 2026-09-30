/* Win98 telemetry writer. The desktop reads a local data file, never ActiveX. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "telemetry.h"
#include "cpucache.h"
#ifdef W98_SUITE
#include "extras.h"
#include "dosquiet.h"
#define WRITER_INTERVAL 250
#else
#define WRITER_INTERVAL 1000
#endif

static char directory[MAX_PATH], destination[MAX_PATH], temporary[MAX_PATH];
static TELEMETRY sample;
#ifdef W98_SUITE
static int coreRunning = -1, quietMode, quietPublished = -1;
static DWORD corePollTick;
static int pauseDos = 1;
static void loadQuietOption(void)
{
    char path[MAX_PATH];
    sprintf(path, "%s\\WIDGETS.INI", directory);
    pauseDos = GetPrivateProfileIntA("Options", "pauseDos", 1, path) != 0;
}
static void updateQuiet(void);
static void publishQuiet(void)
{
    char path[MAX_PATH], temporaryPath[MAX_PATH];
    FILE *file;
    int ok;
    if (quietPublished == quietMode)
        return;
    sprintf(path, "%s\\RUNSTATE.JS", directory);
    sprintf(temporaryPath, "%s\\RUNSTATE.TMP", directory);
    file = fopen(temporaryPath, "wb");
    if (!file)
        return;
    ok = fprintf(file, "var desktopPaused=%d;\r\n", quietMode) > 0;
    if (fclose(file))
        ok = 0;
    if (ok && (DeleteFileA(path) || GetLastError() == ERROR_FILE_NOT_FOUND) && MoveFileA(temporaryPath, path))
        quietPublished = quietMode;
}
static void updateQuiet(void)
{
    int state = pauseDos ? dos_fullscreen() : 0;
    if (state >= 0 && state != quietMode)
    {
        quietMode = state;
        extra_pause(quietMode);
        if (quietMode)
        {
            telemetry_stop(&sample);
            coreRunning = -1;
        }
    }
    publishQuiet();
}
static void updateCore(void);
#endif
static const char *windowClass = "W98WidgetsDataWriter";
static const char *runKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static void writeSnapshot(void)
{
    char snapshot[512], output[1400], *p;
    const char *s;
    FILETIME time;
    double milliseconds;
    DWORD written;
    HANDLE file;
    telemetry_text(&sample, snapshot);
    GetSystemTimeAsFileTime(&time);
    milliseconds = ((double)time.dwHighDateTime * 4294967296.0 + time.dwLowDateTime) / 10000.0 - 11644473600000.0;
    p = output + sprintf(output, "var sampleTime=%.0f;var snapshot='", milliseconds);
    for (s = snapshot; *s; s++)
    {
        if (*s == '\'' || *s == '\\')
            *p++ = '\\';
        *p++ = (unsigned char) * s < 32 || (unsigned char) * s > 126 ? '?' : *s;
    }
    strcpy(p, "';\r\n");
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    if (!WriteFile(file, output, strlen(output), &written, NULL) || written != strlen(output))
    {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    /* Win9x has no ReplaceFile. Publish only a fully written file. The HTML
       remains present, handles a missing snapshot, and refreshes to retry. */
    SetFileAttributesA(destination, FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileA(destination) && GetLastError() != ERROR_FILE_NOT_FOUND)
        return;
    MoveFileA(temporary, destination);
}

static LRESULT CALLBACK writerWindow(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
#ifdef W98_SUITE
    if (message == WM_APP + 1)
    {
        loadQuietOption();
        updateQuiet();
        extra_refresh();
        updateCore();
        return 0;
    }
    if (message == WM_COPYDATA)
    {
        COPYDATASTRUCT *data = (COPYDATASTRUCT*)lp;
        if (data && data->dwData == 0x9832 && data->cbData > 0 && data->cbData < 128 && data->lpData &&
                ((char*)data->lpData)[data->cbData - 1] == 0)
            return device_action((char*)data->lpData);
        return 0;
    }
#endif
    if (message == WM_TIMER)
    {
#ifdef W98_SUITE
        updateQuiet();
        if (coreRunning < 0 || GetTickCount() - corePollTick >= 1000)
            updateCore();
#else
        writeSnapshot();
#endif
        return 0;
    }
    if (message == WM_QUERYENDSESSION)
        return TRUE;
    if (message == WM_CLOSE || (message == WM_ENDSESSION && wp))
    {
#ifdef W98_SUITE
        /* Finish media cleanup before acknowledging Windows session termination. */
        extra_stop();
#endif
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wp, lp);
}

#ifdef W98_SUITE
static void updateCore(void)
{
    if (quietMode)
        return;
    corePollTick = GetTickCount();
    if (extra_core_enabled())
    {
        if (coreRunning != 1)
        {
            telemetry_init(&sample);
            cpu_frequency_cache(&sample, directory);
            coreRunning = 1;
        }
        writeSnapshot();
    }
    else if (coreRunning != 0)
    {
        if (coreRunning == 1)
            telemetry_stop(&sample);
        coreRunning = 0;
        DeleteFileA(destination);
    }
}
#endif

static int registration(const char *image, BOOL install)
{
    HKEY key;
    DWORD disposition;
    LONG error;
    char command[MAX_PATH + 4];
    HWND running;
    DWORD pid;
    HANDLE process;
    STARTUPINFOA startup;
    PROCESS_INFORMATION child;
    error = RegCreateKeyExA(HKEY_CURRENT_USER, runKey, 0, NULL, 0, KEY_WRITE, NULL, &key, &disposition);
    if (error)
        return 1;
    if (install)
    {
        wsprintfA(command, "\"%s\"", image);
        error = RegSetValueExA(key, "W98WidgetsTelemetry", 0, REG_SZ, (const BYTE*)command, strlen(command) + 1);
    }
    else
    {
        error = RegDeleteValueA(key, "W98WidgetsTelemetry");
        if (error == ERROR_FILE_NOT_FOUND)
            error = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (error)
        return 2;
    if (install)
    {
        memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        if (!CreateProcessA(image, command, NULL, NULL, FALSE, 0, NULL, directory, &startup, &child))
            return 3;
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
    }
    else
    {
        running = FindWindowA(windowClass, NULL);
        if (running)
        {
            GetWindowThreadProcessId(running, &pid);
            process = OpenProcess(SYNCHRONIZE, FALSE, pid);
            PostMessageA(running, WM_CLOSE, 0, 0);
            if (process)
            {
                DWORD result = WaitForSingleObject(process, 5000);
                CloseHandle(process);
                if (result != WAIT_OBJECT_0)
                    return 4;
            }
        }
        /* Invalidates any leftover page instead of leaving stale live values. */
        SetFileAttributesA(destination, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(destination);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    char image[MAX_PATH], *slash;
    DWORD length;
    HANDLE mutex;
    WNDCLASSA wc;
    HWND window;
    MSG message;
    (void)previous;
    (void)show;
    length = GetModuleFileNameA(NULL, image, sizeof(image));
    if (!length || length >= sizeof(image))
        return 1;
    strcpy(directory, image);
    slash = strrchr(directory, '\\');
    if (!slash)
        return 1;
    *slash = 0;
    if (strlen(directory) > MAX_PATH - 14)
        return 1;
    wsprintfA(destination, "%s\\DATA.JS", directory);
    wsprintfA(temporary, "%s\\DATA.TMP", directory);
    if (!lstrcmpiA(command, "/register"))
        return registration(image, TRUE);
    if (!lstrcmpiA(command, "/unregister"))
        return registration(image, FALSE);
    if (*command)
        return 2;
    mutex = CreateMutexA(NULL, FALSE, "W98WidgetsTelemetryWriter");
    if (!mutex)
        return 3;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return 0;
    }
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = writerWindow;
    wc.hInstance = instance;
    wc.lpszClassName = windowClass;
    if (!RegisterClassA(&wc))
    {
        CloseHandle(mutex);
        return 4;
    }
    window = CreateWindowA(windowClass, "Glass98 telemetry", 0, 0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!window)
    {
        CloseHandle(mutex);
        return 5;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#ifdef W98_SUITE
    loadQuietOption();
    updateQuiet();
    extra_start(directory);
    updateCore();
#else
    telemetry_init(&sample);
    cpu_frequency_cache(&sample, directory);
    writeSnapshot();
#endif
    if (!SetTimer(window, 1, WRITER_INTERVAL, NULL))
    {
        telemetry_stop(&sample);
        DestroyWindow(window);
        CloseHandle(mutex);
        return 6;
    }
    while (GetMessageA(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    KillTimer(window, 1);
#ifdef W98_SUITE
    extra_stop();
    quietMode = 0;
    publishQuiet();
#endif
    telemetry_stop(&sample);
    DestroyWindow(window);
    CloseHandle(mutex);
    return 0;
}
