/* Active Desktop wallpaper installer. Does not alter IE security settings. */
#define _WIN32_IE 0x0500
#define COBJMACROS
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const CLSID cls = {0x75048700, 0xef1f, 0x11d0, {0x98, 0x88, 0, 0x60, 0x97, 0xde, 0xac, 0xf9}};
static const IID iid = {0xf490eb00, 0x1240, 0x11d1, {0x98, 0x88, 0, 0x60, 0x97, 0xde, 0xac, 0xf9}};
static int compatible(void)
{
    OSVERSIONINFOA os;
    HKEY key;
    char version[80];
    DWORD n = sizeof(version), type;
    HDC dc;
    int bits, major = 0, minor = 0, build = 0;
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    if (os.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS || os.dwMajorVersion != 4 || os.dwMinorVersion != 10 ||
            (os.dwBuildNumber & 0xffff) != 2222)
    {
        puts("Glass98 requires Windows 98 SE.");
        return 0;
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Internet Explorer", 0, KEY_READ, &key))
    {
        puts("Cannot determine IE version.");
        return 0;
    }
    memset(version, 0, sizeof(version));
    type = 0;
    if (RegQueryValueExA(key, "Version", NULL, &type, (BYTE*)version, &n) || type != REG_SZ)
    {
        RegCloseKey(key);
        return 0;
    }
    RegCloseKey(key);
    version[sizeof(version) - 1] = 0;
    sscanf(version, "%d.%d.%d", &major, &minor, &build);
    if (major < 6 || (major == 6 && build < 2800))
    {
        puts("Glass requires Internet Explorer 6 SP1. Use the classic widget on IE5.");
        return 0;
    }
    dc = GetDC(NULL);
    bits = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
    ReleaseDC(NULL, dc);
    if (bits < 16)
    {
        puts("Select High Color (16-bit) or True Color (24/32-bit) first.");
        return 0;
    }
    printf("Windows 98 SE; IE %s; display %d-bit.\n", version, bits);
    return 1;
}
int main(int argc, char **argv)
{
    char page[MAX_PATH], backup[MAX_PATH], current[MAX_PATH], original[MAX_PATH], styleText[20], settings[MAX_PATH], *slash,
         *ext;
    WCHAR wide[MAX_PATH];
    DWORD length;
    IActiveDesktop *ad;
    WALLPAPEROPT opt;
    COMPONENTSOPT co;
    HRESULT hr;
    int install, seed, active, result = 1;
    if (argc != 3)
    {
        puts("Usage: glassctl check|seed|install|remove <GLASS.HTM>");
        return 2;
    }
    if (!strcmp(argv[1], "check"))
        return compatible() ? 0 : 3;
    seed = !strcmp(argv[1], "seed");
    install = !strcmp(argv[1], "install");
    if (!install && !seed && strcmp(argv[1], "remove"))
        return 2;
    if (install && !compatible())
        return 3;
    length = GetFullPathNameA(argv[2], MAX_PATH, page, NULL);
    if (!length || length >= MAX_PATH)
        return 2;
    strcpy(backup, page);
    slash = strrchr(backup, '\\');
    if (!slash || slash - backup > MAX_PATH - 12)
        return 2;
    strcpy(slash + 1, "GLASS.INI");
    if (install && GetFileAttributesA(page) == INVALID_FILE_ATTRIBUTES)
    {
        puts("GLASS.HTM missing.");
        return 2;
    }
    hr = CoInitialize(NULL);
    if (FAILED(hr))
        return 4;
    hr = CoCreateInstance(&cls, NULL, CLSCTX_INPROC_SERVER, &iid, (void**)&ad);
    if (FAILED(hr))
    {
        CoUninitialize();
        return 4;
    }
    memset(&opt, 0, sizeof(opt));
    opt.dwSize = sizeof(opt);
    hr = ad->lpVtbl->GetWallpaper(ad, wide, MAX_PATH, 0);
    if (FAILED(hr))
        goto done;
    if (!WideCharToMultiByte(CP_ACP, 0, wide, -1, current, MAX_PATH, NULL, NULL))
        goto done;
    hr = ad->lpVtbl->GetWallpaperOptions(ad, &opt, 0);
    if (FAILED(hr))
        goto done;
    if (seed)
    {
        strcpy(settings, page);
        strcpy(strrchr(settings, '\\') + 1, "WIDGETS.INI");
        GetPrivateProfileStringA("Desktop", "Wallpaper", "", original, sizeof(original), settings);
        if (*original)
        {
            puts("Keeping configured Glass98 wallpaper.");
            result = 0;
            goto done;
        }
        ext = strrchr(current, '.');
        if (!*current || !ext || (lstrcmpiA(ext, ".bmp") && lstrcmpiA(ext, ".jpg") && lstrcmpiA(ext, ".jpeg") &&
                                  lstrcmpiA(ext, ".gif") && lstrcmpiA(ext, ".png")) || GetFileAttributesA(current) == INVALID_FILE_ATTRIBUTES)
        {
            strcpy(current, "WALL.BMP");
            opt.dwStyle = 2;
            puts("Using solid Windows 98 teal fallback.");
        }
        else
            printf("Using existing Windows wallpaper: %s\n", current);
        sprintf(styleText, "%lu", opt.dwStyle <= 2 ? opt.dwStyle : 0);
        if (!WritePrivateProfileStringA("Desktop", "Wallpaper", current, settings) ||
                !WritePrivateProfileStringA("Desktop", "InitialWallpaperMode", styleText, settings))
            goto done;
        result = 0;
        goto done;
    }
    active = GetPrivateProfileIntA("Original", "Active", 0, backup);
    if (install)
    {
        if (!lstrcmpiA(current, page))
        {
            hr = ad->lpVtbl->ApplyChanges(ad, AD_APPLY_ALL | AD_APPLY_FORCE);
            if (FAILED(hr))
                goto done;
            puts("Glass98 desktop refreshed; backup preserved.");
            result = 0;
            goto done;
        }
        if (active)
        {
            puts("A previous glass backup exists. Run REMOVE.BAT before reinstalling.");
            goto done;
        }
        sprintf(styleText, "%lu", opt.dwStyle);
        if (!WritePrivateProfileStringA("Original", "Wallpaper", current, backup) ||
                !WritePrivateProfileStringA("Original", "Style", styleText, backup) ||
                !WritePrivateProfileStringA("Original", "Active", "1", backup))
        {
            puts("Could not save wallpaper backup.");
            goto done;
        }
        MultiByteToWideChar(CP_ACP, 0, page, -1, wide, MAX_PATH);
        hr = ad->lpVtbl->SetWallpaper(ad, wide, 0);
        if (FAILED(hr))
            goto done;
        memset(&co, 0, sizeof(co));
        co.dwSize = sizeof(co);
        co.fActiveDesktop = TRUE;
        co.fEnableComponents = TRUE;
        hr = ad->lpVtbl->SetDesktopItemOptions(ad, &co, 0);
        if (FAILED(hr))
            goto done;
        hr = ad->lpVtbl->ApplyChanges(ad, AD_APPLY_ALL);
        if (FAILED(hr))
            goto done;
        puts("Glass98 desktop installed. Previous wallpaper saved in GLASS.INI.");
        result = 0;
    }
    else
    {
        if (!active)
        {
            puts("No active glass installation recorded.");
            result = 0;
            goto done;
        }
        if (!lstrcmpiA(current, page))
        {
            GetPrivateProfileStringA("Original", "Wallpaper", "", original, MAX_PATH, backup);
            opt.dwStyle = GetPrivateProfileIntA("Original", "Style", 0, backup);
            if (opt.dwStyle > 2)
            {
                puts("Invalid saved wallpaper style.");
                goto done;
            }
            MultiByteToWideChar(CP_ACP, 0, original, -1, wide, MAX_PATH);
            hr = ad->lpVtbl->SetWallpaper(ad, wide, 0);
            if (FAILED(hr))
                goto done;
            hr = ad->lpVtbl->SetWallpaperOptions(ad, &opt, 0);
            if (FAILED(hr))
                goto done;
            hr = ad->lpVtbl->ApplyChanges(ad, AD_APPLY_ALL);
            if (FAILED(hr))
                goto done;
            puts("Previous wallpaper and style restored.");
        }
        else
            puts("Wallpaper was changed separately; keeping that selection.");
        if (!WritePrivateProfileStringA("Original", "Active", "0", backup))
            goto done;
        result = 0;
    }
done:
    if (result)
        printf("Glass operation failed (HRESULT %08lx). Keep GLASS.INI for recovery.\n", hr);
    ad->lpVtbl->Release(ad);
    CoUninitialize();
    return result;
}
