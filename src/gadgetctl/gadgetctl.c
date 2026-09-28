#define _WIN32_IE 0x0500
#define COBJMACROS
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

static const CLSID desktopClass = {0x75048700, 0xef1f, 0x11d0, {0x98, 0x88, 0, 0x60, 0x97, 0xde, 0xac, 0xf9}};
static const IID desktopInterface = {0xf490eb00, 0x1240, 0x11d1, {0x98, 0x88, 0, 0x60, 0x97, 0xde, 0xac, 0xf9}};

/* Win9x exports Unicode NLS functions as stubs. Compare UTF-16 code units here. */
static int sameSource(const WCHAR *a, const WCHAR *b)
{
    unsigned i;
    for (i = 0; i < INTERNET_MAX_URL_LENGTH; i++)
    {
        if (a[i] != b[i])
            return 0;
        if (!a[i])
            return 1;
    }
    return 0;
}

static int failure(const char *operation, HRESULT hr)
{
    printf("ERROR %s: 0x%08lX\n", operation, (unsigned long)hr);
    return 1;
}

static int registration(const char *command, const char *path)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD code;
    char absolute[MAX_PATH], line[MAX_PATH + 40];
    DWORD length;
    length = GetFullPathNameA(path, sizeof(absolute), absolute, NULL);
    if (!length || length >= sizeof(absolute))
        return failure("DLL path", E_INVALIDARG);
    wsprintfA(line, "\"%s\" /%s", absolute, command);
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    if (!CreateProcessA(absolute, line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
        return failure("bridge registrar", HRESULT_FROM_WIN32(GetLastError()));
    if (WaitForSingleObject(process.hProcess, 10000) != WAIT_OBJECT_0)
    {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return failure("registrar timeout", E_FAIL);
    }
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (code)
        return failure(command, E_FAIL);
    printf("Bridge %s succeeded.\n", command);
    return 0;
}

int main(int argc, char **argv)
{
    IActiveDesktop *desktop = NULL;
    COMPONENT item;
    COMPONENTSOPT options;
    WCHAR source[INTERNET_MAX_URL_LENGTH];
    char path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    DWORD pathLength, urlLength = sizeof(url), itemSize = sizeof(IE4COMPONENT);
    HRESULT hr;
    int count = 0, index, found = 0, result = 0, enable = 0;
    const char *command;
    if (argc < 3)
    {
        puts("Usage: gadgetctl install|status|uninstall <system.htm> [--enable] [--modern]\n"
             "       gadgetctl register|unregister <w98data.exe>\n"
             "Use --enable with install to activate Active Desktop explicitly.\n"
             "Default structure: IE4COMPONENT; --modern is a compatibility probe.");
        return 2;
    }
    command = argv[1];
    if (!strcmp(command, "register") || !strcmp(command, "unregister"))
        return registration(command, argv[2]);
    if (strcmp(command, "install") && strcmp(command, "status") && strcmp(command, "uninstall"))
        return 2;
    for (index = 3; index < argc; ++index)
    {
        if (!strcmp(argv[index], "--enable"))
            enable = 1;
        else if (!strcmp(argv[index], "--modern"))
            itemSize = sizeof(COMPONENT);
        else
            return 2;
    }
    pathLength = GetFullPathNameA(argv[2], sizeof(path), path, NULL);
    if (!pathLength || pathLength >= sizeof(path))
        return failure("path", HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    hr = UrlCreateFromPathA(path, url, &urlLength, 0);
    if (FAILED(hr))
        return failure("file URL", hr);
    if (!MultiByteToWideChar(CP_ACP, 0, url, -1, source, INTERNET_MAX_URL_LENGTH))
        return 1;
    printf("Source: %s\nStructure size: %lu\n", url, (unsigned long)itemSize);
    hr = CoInitialize(NULL);
    if (FAILED(hr))
        return failure("CoInitialize", hr);
    hr = CoCreateInstance(&desktopClass, NULL, CLSCTX_INPROC_SERVER, &desktopInterface, (void**)&desktop);
    if (FAILED(hr))
    {
        CoUninitialize();
        return failure("IActiveDesktop", hr);
    }
    memset(&options, 0, sizeof(options));
    options.dwSize = sizeof(options);
    hr = desktop->lpVtbl->GetDesktopItemOptions(desktop, &options, 0);
    if (FAILED(hr))
    {
        result = failure("GetDesktopItemOptions", hr);
        goto done;
    }
    printf("ActiveDesktop: %d; components enabled: %d\n", options.fActiveDesktop, options.fEnableComponents);
    hr = desktop->lpVtbl->GetDesktopItemCount(desktop, &count, 0);
    if (FAILED(hr))
    {
        result = failure("GetDesktopItemCount", hr);
        goto done;
    }
    printf("Desktop items: %d\n", count);
    for (index = 0; index < count; ++index)
    {
        memset(&item, 0, sizeof(item));
        item.dwSize = itemSize;
        item.cpPos.dwSize = sizeof(COMPPOS);
        hr = desktop->lpVtbl->GetDesktopItem(desktop, index, &item, 0);
        if (FAILED(hr))
        {
            result = failure("GetDesktopItem", hr);
            goto done;
        }
        if (sameSource(item.wszSource, source))
        {
            found = 1;
            break;
        }
    }
    if (found)
        printf("Installed: enabled=%d x=%d y=%d width=%lu height=%lu type=%d\n",
               item.fChecked, item.cpPos.iLeft, item.cpPos.iTop, item.cpPos.dwWidth, item.cpPos.dwHeight, item.iComponentType);
    else
        puts("Not installed.");
    if (!strcmp(command, "status"))
        goto done;
    if (!strcmp(command, "install") && found && options.fActiveDesktop && options.fEnableComponents)
    {
        puts("Already installed; no desktop refresh needed.");
        goto done;
    }
    if (!strcmp(command, "uninstall"))
    {
        if (!found)
            goto done;
        hr = desktop->lpVtbl->RemoveDesktopItem(desktop, &item, 0);
        if (FAILED(hr))
        {
            result = failure("RemoveDesktopItem", hr);
            goto done;
        }
    }
    else
    {
        if ((!options.fActiveDesktop || !options.fEnableComponents) && !enable)
        {
            puts("Active Desktop/components disabled. Repeat install with --enable to activate.");
            result = 3;
            goto done;
        }
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        {
            result = failure("HTML missing", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            goto done;
        }
        if (!found)
        {
            memset(&item, 0, sizeof(item));
            item.dwSize = itemSize;
            item.iComponentType = COMP_TYPE_WEBSITE;
            item.fChecked = TRUE;
            item.fNoScroll = TRUE;
            item.cpPos.dwSize = sizeof(COMPPOS);
            item.cpPos.iLeft = GetSystemMetrics(SM_CXSCREEN) - 272;
            if (item.cpPos.iLeft < 0)
                item.cpPos.iLeft = 0;
            item.cpPos.iTop = 24;
            item.cpPos.dwWidth = 248;
            item.cpPos.dwHeight = 370;
            item.cpPos.izIndex = 0;
            item.cpPos.fCanResize = FALSE;
            memcpy(item.wszSource, source, sizeof(source));
            MultiByteToWideChar(CP_ACP, 0, "Glass98 System", -1, item.wszFriendlyName, MAX_PATH);
            hr = desktop->lpVtbl->AddDesktopItem(desktop, &item, 0);
            if (FAILED(hr))
            {
                result = failure("AddDesktopItem", hr);
                goto done;
            }
        }
        if (enable && (!options.fActiveDesktop || !options.fEnableComponents))
        {
            options.fActiveDesktop = TRUE;
            options.fEnableComponents = TRUE;
            hr = desktop->lpVtbl->SetDesktopItemOptions(desktop, &options, 0);
            if (FAILED(hr))
            {
                result = failure("SetDesktopItemOptions", hr);
                goto done;
            }
        }
    }
    hr = desktop->lpVtbl->ApplyChanges(desktop, AD_APPLY_ALL);
    if (FAILED(hr))
        result = failure("ApplyChanges", hr);
    else
        puts("Changes applied.");
done:
    desktop->lpVtbl->Release(desktop);
    CoUninitialize();
    return result;
}
