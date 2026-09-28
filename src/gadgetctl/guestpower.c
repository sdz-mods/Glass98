/* Request normal Windows shutdown/reboot so applications can save state. */
#include <windows.h>
int WINAPI WinMain(HINSTANCE a, HINSTANCE b, LPSTR command, int show)
{
    UINT flags;
    (void)a;
    (void)b;
    (void)show;
    if (!lstrcmpiA(command, "/reboot"))
        flags = EWX_REBOOT;
    else if (!lstrcmpiA(command, "/shutdown"))
        flags = EWX_SHUTDOWN | EWX_POWEROFF;
    else
        return 2;
    return ExitWindowsEx(flags, 0) ? 0 : 1;
}
