/* Win16 USER exposes the actual Win9x resource percentages. No thunk DLL needed.
   Hidden, demand-started by W98DATA; exits when that process goes away. */
#include <windows.h>
static LRESULT CALLBACK resource_window(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_USER + 98)
        return (LONG)GetFreeSystemResources(GFSR_SYSTEMRESOURCES) | ((LONG)GetFreeSystemResources(GFSR_USERRESOURCES) << 8) | ((
                    LONG)GetFreeSystemResources(GFSR_GDIRESOURCES) << 16) | 0x01000000L;
    if (m == WM_TIMER && !FindWindow("W98WidgetsDataWriter", NULL))
    {
        DestroyWindow(w);
        return 0;
    }
    if (m == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(w, m, wp, lp);
}
int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND w;
    MSG msg;
    (void)cmd;
    (void)show;
    if (FindWindow("W98Resources16", NULL))
        return 0;
    if (!previous)
    {
        wc.style = 0;
        wc.lpfnWndProc = resource_window;
        wc.cbClsExtra = wc.cbWndExtra = 0;
        wc.hInstance = instance;
        wc.hIcon = 0;
        wc.hCursor = 0;
        wc.hbrBackground = 0;
        wc.lpszMenuName = 0;
        wc.lpszClassName = "W98Resources16";
        if (!RegisterClass(&wc))
            return 1;
    }
    w = CreateWindow("W98Resources16", "", WS_OVERLAPPED, 0, 0, 0, 0, 0, 0, instance, 0);
    if (!w)
        return 1;
    SetTimer(w, 1, 2000, NULL);
    while (GetMessage(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
