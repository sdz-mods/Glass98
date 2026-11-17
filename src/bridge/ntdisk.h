/* XP file-I/O rates. Resolve English counter IDs to local names once per query. */
#ifndef GLASS_NTDISK_H
#define GLASS_NTDISK_H
#include <stdlib.h>
typedef LONG(WINAPI *NT_OPEN_QUERY)(LPCSTR, DWORD, HANDLE *);
typedef LONG(WINAPI *NT_ADD_COUNTER)(HANDLE, LPCSTR, DWORD, HANDLE *);
typedef LONG(WINAPI *NT_QUERY)(HANDLE);
typedef LONG(WINAPI *NT_LOOKUP)(LPCSTR, DWORD, LPSTR, LPDWORD);
#pragma pack(push, 8)
typedef struct
{
    DWORD status;
    double value;
} NT_COUNTER_VALUE;
#pragma pack(pop)
typedef LONG(WINAPI *NT_FORMAT)(HANDLE, DWORD, LPDWORD, NT_COUNTER_VALUE *);
static HMODULE diskLibrary;
static HANDLE diskQuery, diskCounters[2];
static NT_QUERY diskCollect, diskClose;
static NT_FORMAT diskFormat;
static DWORD diskRetry;
static void nt_disk_stop(void)
{
    if (diskQuery)
        diskClose(diskQuery);
    if (diskLibrary)
        FreeLibrary(diskLibrary);
    diskQuery = NULL;
    diskLibrary = NULL;
    diskRetry = 0;
}
static int nt_counter_name(NT_LOOKUP lookup, const char *english, char *out)
{
    HKEY key;
    char *data, *id, *name;
    DWORD size = 0, length = 256, type;
    int ok = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009", 0, KEY_READ, &key))
        return 0;
    if (!RegQueryValueExA(key, "Counter", NULL, &type, NULL, &size) &&
            type == REG_MULTI_SZ && size > 1 && size <= 1048576)
    {
        data = calloc(1, size + 2);
        if (data)
        {
            if (!RegQueryValueExA(key, "Counter", NULL, &type, (BYTE *)data, &size))
                for (id = data; *id; id = name + strlen(name) + 1)
                {
                    name = id + strlen(id) + 1;
                    if (!*name)
                        break;
                    if (!strcmp(name, english))
                    {
                        ok = lookup(NULL, strtoul(id, NULL, 10), out, &length) == 0;
                        break;
                    }
                }
            free(data);
        }
    }
    RegCloseKey(key);
    return ok;
}
static void nt_disk_read(double *rates)
{
    NT_OPEN_QUERY open;
    NT_ADD_COUNTER add;
    NT_LOOKUP lookup;
    NT_COUNTER_VALUE value;
    char object[256], name[256], path[600];
    const char *names[2] = {"File Read Bytes/sec", "File Write Bytes/sec"};
    int i;
    DWORD now = GetTickCount();
    if (!diskQuery)
    {
        if (diskRetry && now - diskRetry < 60000)
            return;
        diskLibrary = LoadLibraryA("PDH.DLL");
        if (!diskLibrary)
            goto failed;
        open = (NT_OPEN_QUERY)GetProcAddress(diskLibrary, "PdhOpenQueryA");
        add = (NT_ADD_COUNTER)GetProcAddress(diskLibrary, "PdhAddCounterA");
        lookup = (NT_LOOKUP)GetProcAddress(diskLibrary, "PdhLookupPerfNameByIndexA");
        diskCollect = (NT_QUERY)GetProcAddress(diskLibrary, "PdhCollectQueryData");
        diskClose = (NT_QUERY)GetProcAddress(diskLibrary, "PdhCloseQuery");
        diskFormat = (NT_FORMAT)GetProcAddress(diskLibrary, "PdhGetFormattedCounterValue");
        if (!open || !add || !lookup || !diskCollect || !diskClose || !diskFormat)
            goto failed;
        if (open(NULL, 0, &diskQuery) || !nt_counter_name(lookup, "System", object))
            goto failed;
        for (i = 0; i < 2; i++)
        {
            if (!nt_counter_name(lookup, names[i], name))
                goto failed;
            sprintf(path, "\\%s\\%s", object, name);
            if (add(diskQuery, path, 0, &diskCounters[i]))
                goto failed;
        }
        diskCollect(diskQuery); /* Rates require two samples. */
        return;
    }
    if (diskCollect(diskQuery))
        goto failed;
    for (i = 0; i < 2; i++)
        if (!diskFormat(diskCounters[i], 0x200, NULL, &value) && value.status <= 1 && value.value >= 0)
            rates[i] = value.value;
    return;
failed:
    nt_disk_stop();
    diskRetry = now;
}
#endif
