/* XP APIs are resolved at runtime so the same executable loads on Win98. */
#ifndef GLASS_NTSTATS_H
#define GLASS_NTSTATS_H
#include <windows.h>
typedef BOOL (WINAPI *GLASS_SYSTEM_TIMES)(LPFILETIME, LPFILETIME, LPFILETIME);
typedef struct
{
    DWORD length, load;
    unsigned __int64 physical, available, commit, commitAvailable;
    unsigned __int64 virtualTotal, virtualAvailable, extendedAvailable;
} GLASS_MEMORY_EX;
typedef BOOL (WINAPI *GLASS_MEMORY_STATUS)(GLASS_MEMORY_EX *);
typedef struct
{
    DWORD dwMemoryLoad;
    double dwTotalPhys, dwAvailPhys, dwTotalPageFile, dwAvailPageFile;
    double dwTotalVirtual, dwAvailVirtual;
} GLASS_MEMORY;
static double glass_filetime(FILETIME time)
{
    return time.dwHighDateTime * 4294967296.0 + time.dwLowDateTime;
}
static LONG glass_cpu_percent(double oldIdle, double oldTotal, double idle, double total)
{
    double elapsed = total - oldTotal, busy = elapsed - (idle - oldIdle);
    if (elapsed <= 0 || idle < oldIdle || busy < 0 || busy > elapsed)
        return -1;
    return (LONG)(busy * 100.0 / elapsed + 0.5);
}
static int glass_system_times(double *idle, double *total)
{
    static GLASS_SYSTEM_TIMES read;
    FILETIME i, k, u;
    if (!read)
        read = (GLASS_SYSTEM_TIMES)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "GetSystemTimes");
    if (!read || !read(&i, &k, &u))
        return 0;
    *idle = glass_filetime(i);
    *total = glass_filetime(k) + glass_filetime(u);
    return 1;
}
static void glass_memory(GLASS_MEMORY *out)
{
    GLASS_MEMORY_EX ex;
    MEMORYSTATUS old;
    GLASS_MEMORY_STATUS read;
    read = (GLASS_MEMORY_STATUS)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "GlobalMemoryStatusEx");
    ex.length = sizeof(ex);
    if (!(GetVersion() & 0x80000000UL) && read && read(&ex))
    {
        out->dwMemoryLoad = ex.load;
        out->dwTotalPhys = (double)ex.physical;
        out->dwAvailPhys = (double)ex.available;
        out->dwTotalPageFile = (double)ex.commit;
        out->dwAvailPageFile = (double)ex.commitAvailable;
        out->dwTotalVirtual = (double)ex.virtualTotal;
        out->dwAvailVirtual = (double)ex.virtualAvailable;
    }
    else
    {
        old.dwLength = sizeof(old);
        GlobalMemoryStatus(&old);
        out->dwMemoryLoad = old.dwMemoryLoad;
        out->dwTotalPhys = old.dwTotalPhys;
        out->dwAvailPhys = old.dwAvailPhys;
        out->dwTotalPageFile = old.dwTotalPageFile;
        out->dwAvailPageFile = old.dwAvailPageFile;
        out->dwTotalVirtual = old.dwTotalVirtual;
        out->dwAvailVirtual = old.dwAvailVirtual;
    }
}
#endif
