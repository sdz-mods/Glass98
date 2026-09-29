/* Explicit user actions run out of process so DNS/media drivers cannot stall telemetry. */
#include <winsock.h>
#include <windows.h>
#include <ipexport.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static void quoted(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char*)s;
    fputc('\'', f);
    for (; *p; p++)
    {
        if (*p < 32 || *p >= 127 || *p == '\'' || *p == '\\' || *p == '<')
            fprintf(f, "\\x%02x", *p);
        else
            fputc(*p, f);
    }
    fputc('\'', f);
}
static void ping_result(const char *root, const char *target, const char *message, int ms)
{
    char temp[MAX_PATH], dest[MAX_PATH];
    FILE *f;
    FILETIME ft;
    double now;
    int ok;
    sprintf(temp, "%s\\PING.TMP", root);
    sprintf(dest, "%s\\PING.JS", root);
    f = fopen(temp, "wb");
    if (!f)
        return;
    GetSystemTimeAsFileTime(&ft);
    now = ((double)ft.dwHighDateTime * 4294967296.0 + ft.dwLowDateTime) / 10000.0 - 11644473600000.0;
    fprintf(f, "var pingResult={time:%.0f,ms:%d,target:", now, ms);
    quoted(f, target);
    fputs(",message:", f);
    quoted(f, message);
    fputs("};\n", f);
    ok = !ferror(f);
    if (fclose(f))
        ok = 0;
    if (ok && (DeleteFileA(dest) || GetLastError() == ERROR_FILE_NOT_FOUND))
        MoveFileA(temp, dest);
}
static int ping_host(const char *root, const char *host)
{
    WSADATA ws;
    struct hostent *h;
    IPAddr addr;
    HMODULE module;
    HANDLE handle;
    char reply[512], message[160];
    DWORD n, status;
    int ms = -1;
    HANDLE(WINAPI * create)(void);
    BOOL (WINAPI * close)(HANDLE);
    DWORD (WINAPI * send)(HANDLE, IPAddr, LPVOID, WORD, PIP_OPTION_INFORMATION, LPVOID, DWORD, DWORD);
    HANDLE lock;
    lock = CreateMutexA(NULL, FALSE, "W98WidgetPing");
    if (!lock)
        return 0;
    if (WaitForSingleObject(lock, 0) != WAIT_OBJECT_0)
    {
        CloseHandle(lock);
        return 1;
    }
    if (!*host)
    {
        ping_result(root, "", "Set a hostname or IPv4 address in widget options", -1);
        goto done;
    }
    ping_result(root, host, "Resolving / checking...", -1);
    if (WSAStartup(MAKEWORD(1, 1), &ws))
    {
        ping_result(root, host, "TCP/IP unavailable", -1);
        goto done;
    }
    addr = inet_addr(host);
    if (addr == INADDR_NONE)
    {
        h = gethostbyname(host);
        if (!h || h->h_length != 4)
        {
            ping_result(root, host, "Name lookup failed", -1);
            WSACleanup();
            goto done;
        }
        memcpy(&addr, h->h_addr_list[0], 4);
    }
    module = LoadLibraryA("ICMP.DLL");
    if (!module)
    {
        ping_result(root, host, "ICMP.DLL unavailable", -1);
        WSACleanup();
        goto done;
    }
    create = (void*)GetProcAddress(module, "IcmpCreateFile");
    close = (void*)GetProcAddress(module, "IcmpCloseHandle");
    send = (void*)GetProcAddress(module, "IcmpSendEcho");
    strcpy(message, "ICMP unavailable");
    if (create && close && send)
    {
        handle = create();
        if (handle != INVALID_HANDLE_VALUE)
        {
            memset(reply, 0, sizeof(reply));
            n = send(handle, addr, "Glass98", sizeof("Glass98") - 1, NULL, reply, sizeof(reply), 1200);
            status = n ? ((PICMP_ECHO_REPLY)reply)->Status : GetLastError();
            if (n && status == 0)
            {
                ms = ((PICMP_ECHO_REPLY)reply)->RoundTripTime;
                strcpy(message, "Reply received");
            }
            else if (status == 11010)
                strcpy(message, "No reply (timeout; ICMP may be blocked)");
            else
                sprintf(message, "No reply (ICMP status %lu)", status);
            close(handle);
        }
    }
    ping_result(root, host, message, ms);
    FreeLibrary(module);
    WSACleanup();
done:
    ReleaseMutex(lock);
    CloseHandle(lock);
    return 1;
}
static DWORD name_id(const char *s)
{
    DWORD h = 2166136261UL;
    for (; *s; s++)
    {
        h ^= (unsigned char) * s;
        h *= 16777619UL;
    }
    return h;
}
static int open_recent(DWORD id)
{
    LPITEMIDLIST item;
    char folder[MAX_PATH], path[MAX_PATH];
    HANDLE search;
    WIN32_FIND_DATAA data;
    int ok = 0;
    if (SHGetSpecialFolderLocation(NULL, CSIDL_RECENT, &item) != NOERROR)
        return 0;
    if (!SHGetPathFromIDListA(item, folder))
    {
        CoTaskMemFree(item);
        return 0;
    }
    CoTaskMemFree(item);
    if (strlen(folder) > MAX_PATH - 8)
        return 0;
    sprintf(path, "%s\\*.lnk", folder);
    search = FindFirstFileA(path, &data);
    if (search == INVALID_HANDLE_VALUE)
        return 0;
    do
    {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name_id(data.cFileName) == id)
        {
            if (strlen(folder) + strlen(data.cFileName) + 2 < MAX_PATH)
            {
                sprintf(path, "%s\\%s", folder, data.cFileName);
                ok = (UINT)ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL) > 32;
            }
            break;
        }
    }
    while (FindNextFileA(search, &data));
    FindClose(search);
    return ok;
}
int desktop_action(const char*, const char*, const char*);
int utility_action(const char *cmd, const char *ini, const char *root)
{
    char value[MAX_PATH], key[32], tail, drive, command[100], *p;
    int slot, ok = 0;
    DWORD id, error;
    OPENFILENAMEA ofn;
    BROWSEINFOA bi;
    LPITEMIDLIST item;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    ok = desktop_action(cmd, ini, root);
    if (ok >= 0)
        return ok;
    ok = 0;
    if (!strcmp(cmd, "ping"))
    {
        GetPrivateProfileStringA("Options", "pingHost", "", value, sizeof(value), ini);
        return ping_host(root, value);
    }
    if (!strncmp(cmd, "pingip/", 7))
    {
        p = (char*)cmd + 7;
        if (strlen(p) > 15)
            return 0;
        for (; *p; p++)
            if (!isdigit((unsigned char)*p) && *p != '.')
                return 0;
        return ping_host(root, cmd + 7);
    }
    if (!strcmp(cmd, "beep"))
    {
        MessageBeep(MB_ICONEXCLAMATION);
        return 1;
    }
    if (!strcmp(cmd, "network"))
        return (UINT)ShellExecuteA(NULL, "open", "CONTROL.EXE", "NETCPL.CPL", NULL, SW_SHOWNORMAL) > 32;
    CoInitialize(NULL);
    if (sscanf(cmd, "photo/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        memset(&ofn, 0, sizeof(ofn));
        value[0] = 0;
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = value;
        ofn.nMaxFile = sizeof(value);
        ofn.lpstrFilter = "Images (*.bmp;*.jpg;*.jpeg;*.gif)\0*.bmp;*.jpg;*.jpeg;*.gif\0\0";
        ofn.lpstrTitle = "Choose a picture frame image";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn))
        {
            sprintf(key, "photo%d", slot);
            ok = WritePrivateProfileStringA("Options", key, value, ini);
        }
        else
            ok = 1;
    }
    else if (sscanf(cmd, "favchoose/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 4)
    {
        memset(&bi, 0, sizeof(bi));
        bi.lpszTitle = "Choose a favorite folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
        item = SHBrowseForFolderA(&bi);
        ok = 1;
        if (item)
        {
            if (SHGetPathFromIDListA(item, value))
            {
                sprintf(key, "fav%d", slot);
                ok = WritePrivateProfileStringA("Options", key, value, ini);
            }
            CoTaskMemFree(item);
        }
    }
    else if (sscanf(cmd, "fav/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 4)
    {
        sprintf(key, "fav%d", slot);
        GetPrivateProfileStringA("Options", key, "", value, sizeof(value), ini);
        if (*value)
            ok = (UINT)ShellExecuteA(NULL, "open", value, NULL, NULL, SW_SHOWNORMAL) > 32;
    }
    else if (sscanf(cmd, "photoclear/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 6)
    {
        sprintf(key, "photo%d", slot);
        ok = WritePrivateProfileStringA("Options", key, "", ini);
    }
    else if (sscanf(cmd, "favclear/%d%c", &slot, &tail) == 1 && slot >= 0 && slot < 4)
    {
        sprintf(key, "fav%d", slot);
        ok = WritePrivateProfileStringA("Options", key, "", ini);
    }
    else if (sscanf(cmd, "recent/%lu%c", &id, &tail) == 1)
        ok = open_recent(id);
    else if (sscanf(cmd, "mediaopen/%c%c", &drive, &tail) == 1 && drive >= 'C' && drive <= 'Z')
    {
        sprintf(value, "%c:\\", drive);
        id = GetDriveTypeA(value);
        if (id == DRIVE_CDROM || id == DRIVE_REMOVABLE)
            ok = (UINT)ShellExecuteA(NULL, "open", value, NULL, NULL, SW_SHOWNORMAL) > 32;
    }
    else if (sscanf(cmd, "eject/%c%c", &drive, &tail) == 1 && drive >= 'C' && drive <= 'Z')
    {
        sprintf(value, "%c:\\", drive);
        if (GetDriveTypeA(value) == DRIVE_CDROM)
        {
            sprintf(command, "open %c: type cdaudio alias w98eject shareable", drive);
            error = mciSendStringA(command, NULL, 0, NULL);
            if (!error)
            {
                ok = mciSendStringA("set w98eject door open", NULL, 0, NULL) == 0;
                mciSendStringA("close w98eject", NULL, 0, NULL);
            }
        }
    }
    CoUninitialize();
    return ok;
}
