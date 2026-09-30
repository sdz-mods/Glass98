/* Win98 fullscreen DOS owns a foreground tty window represented as iconic
   with an empty client rectangle. A minimized background DOS box is not enough. */
#ifndef GLASS98_DOSQUIET_H
#define GLASS98_DOSQUIET_H
static int dos_fullscreen(void)
{
    HWND window;
    RECT client;
    char name[32];
    if (!(GetVersion() & 0x80000000UL))
        return 0;
    window = GetForegroundWindow();
    if (!window || !GetClassNameA(window, name, sizeof(name)))
        return -1; /* Keep the previous state during a foreground transition. */
    if (lstrcmpiA(name, "tty") || !IsIconic(window))
        return 0;
    if (!GetClientRect(window, &client))
        return -1;
    return client.right == client.left && client.bottom == client.top;
}
#endif
