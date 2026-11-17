/* Win98 fullscreen DOS owns a foreground tty window represented as iconic
   with an empty client rectangle. XP reports fullscreen console hardware mode.
   A minimized background DOS box is not enough. */
#ifndef GLASS98_DOSQUIET_H
#define GLASS98_DOSQUIET_H
static int dos_fullscreen(void)
{
    HWND window;
    RECT client;
    char name[32];
    window = GetForegroundWindow();
    if (!window || !GetClassNameA(window, name, sizeof(name)))
        return -1; /* Keep the previous state during a foreground transition. */
    if (!(GetVersion() & 0x80000000UL))
    {
        typedef BOOL (WINAPI * ATTACH)(DWORD);
        typedef BOOL (WINAPI * DISPLAY_MODE)(LPDWORD);
        HMODULE kernel = GetModuleHandleA("KERNEL32.DLL");
        ATTACH attach = (ATTACH)GetProcAddress(kernel, "AttachConsole");
        DISPLAY_MODE mode = (DISPLAY_MODE)GetProcAddress(kernel, "GetConsoleDisplayMode");
        DWORD pid, flags = 0;
        int ok;
        if (lstrcmpiA(name, "ConsoleWindowClass") || !attach || !mode)
            return 0;
        GetWindowThreadProcessId(window, &pid);
        if (!attach(pid))
            return -1;
        /* Attaching resets the handler table. Ignore console close/control events
           while observing another application's console, then detach immediately. */
        SetConsoleCtrlHandler(NULL, TRUE);
        ok = mode(&flags);
        FreeConsole();
        return ok ? (flags & 3) != 0 : -1;
    }
    if (lstrcmpiA(name, "tty") || !IsIconic(window))
        return 0;
    if (!GetClientRect(window, &client))
        return -1;
    return client.right == client.left && client.bottom == client.top;
}
#endif
