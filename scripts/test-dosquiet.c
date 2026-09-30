/* Detector behavior without a DOS VM or host window changes. */
#include <windows.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
static HWND foreground = (HWND)1;
static char windowClass[32] = "tty";
static int iconic = 1, validRect = 1, width, height;
static DWORD version = 0x80000004UL;
static DWORD WINAPI version_stub(void) { return version; }
static HWND WINAPI foreground_stub(void) { return foreground; }
static int WINAPI class_stub(HWND w, LPSTR out, int size) { (void)w; lstrcpynA(out, windowClass, size); return strlen(out); }
static BOOL WINAPI iconic_stub(HWND w) { (void)w; return iconic; }
static BOOL WINAPI rect_stub(HWND w, LPRECT r) { (void)w; SetRect(r, 0, 0, width, height); return validRect; }
#define GetVersion version_stub
#define GetForegroundWindow foreground_stub
#define GetClassNameA class_stub
#define IsIconic iconic_stub
#define GetClientRect rect_stub
#include "../src/bridge/dosquiet.h"
int main(void)
{
    assert(dos_fullscreen() == 1);
    iconic = 0; width = 640; height = 480;
    assert(dos_fullscreen() == 0);
    iconic = 1;
    assert(dos_fullscreen() == 0);
    width = height = 0;
    strcpy(windowClass, "Progman");
    assert(dos_fullscreen() == 0);
    strcpy(windowClass, "tty"); validRect = 0;
    assert(dos_fullscreen() == -1);
    foreground = NULL;
    assert(dos_fullscreen() == -1);
    version = 10;
    assert(dos_fullscreen() == 0);
    puts("PASS: fullscreen DOS, windowed DOS, desktop, transient foreground and unsupported OS");
    return 0;
}
