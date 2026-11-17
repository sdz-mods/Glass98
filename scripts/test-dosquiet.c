/* Detector behavior without a DOS VM or host window changes. */
#include <windows.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static HWND foreground = (HWND)1;
static char windowClass[32] = "tty";
static int iconic = 1, validRect = 1, width, height;
static DWORD version = 0x80000004UL;
static DWORD WINAPI version_stub(void)
{
    return version;
}
static HWND WINAPI foreground_stub(void)
{
    return foreground;
}
static int WINAPI class_stub(HWND w, LPSTR out, int size)
{
    (void)w;
    lstrcpynA(out, windowClass, size);
    return strlen(out);
}
static BOOL WINAPI iconic_stub(HWND w)
{
    (void)w;
    return iconic;
}
static BOOL WINAPI rect_stub(HWND w, LPRECT r)
{
    (void)w;
    SetRect(r, 0, 0, width, height);
    return validRect;
}
static int attachOK = 1, modeOK = 1, detached;
static DWORD consoleFlags;
static BOOL WINAPI attach_stub(DWORD pid)
{
    (void)pid;
    return attachOK;
}
static BOOL WINAPI mode_stub(LPDWORD flags)
{
    *flags = consoleFlags;
    return modeOK;
}
static BOOL WINAPI detach_stub(void)
{
    detached++;
    return TRUE;
}
static BOOL WINAPI handler_stub(PHANDLER_ROUTINE handler, BOOL add)
{
    (void)handler;
    (void)add;
    return TRUE;
}
static DWORD WINAPI pid_stub(HWND w, LPDWORD pid)
{
    (void)w;
    *pid = 7;
    return 1;
}
static FARPROC WINAPI proc_stub(HMODULE module, LPCSTR name)
{
    (void)module;
    if (!strcmp(name, "AttachConsole"))
        return (FARPROC)attach_stub;
    if (!strcmp(name, "GetConsoleDisplayMode"))
        return (FARPROC)mode_stub;
    return NULL;
}
#define GetProcAddress proc_stub
#define FreeConsole detach_stub
#define SetConsoleCtrlHandler handler_stub
#define GetWindowThreadProcessId pid_stub
#define GetVersion version_stub
#define GetForegroundWindow foreground_stub
#define GetClassNameA class_stub
#define IsIconic iconic_stub
#define GetClientRect rect_stub
#include "../src/bridge/dosquiet.h"
int main(void)
{
    assert(dos_fullscreen() == 1);
    iconic = 0;
    width = 640;
    height = 480;
    assert(dos_fullscreen() == 0);
    iconic = 1;
    assert(dos_fullscreen() == 0);
    width = height = 0;
    strcpy(windowClass, "Progman");
    assert(dos_fullscreen() == 0);
    strcpy(windowClass, "tty");
    validRect = 0;
    assert(dos_fullscreen() == -1);
    foreground = NULL;
    assert(dos_fullscreen() == -1);
    version = 10;
    foreground = (HWND)1;
    assert(dos_fullscreen() == 0);
    strcpy(windowClass, "ConsoleWindowClass");
    assert(dos_fullscreen() == 0 && detached == 1);
    consoleFlags = 1;
    assert(dos_fullscreen() == 1 && detached == 2);
    consoleFlags = 3;
    assert(dos_fullscreen() == 1 && detached == 3);
    attachOK = 0;
    assert(dos_fullscreen() == -1 && detached == 3);
    attachOK = 1;
    modeOK = 0;
    assert(dos_fullscreen() == -1 && detached == 4);
    puts("PASS: Win98/XP fullscreen, windowed, transition, failed attachment and detach cleanup");
    return 0;
}
