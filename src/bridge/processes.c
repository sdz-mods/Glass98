/* On-demand process CPU rankings. Lifecycle accounting is off when disabled
   or paused; process/name enumeration occurs only at the widget interval. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "extras.h"
#include "procmeter.h"
#include "processes.h"
#include "ntstats.h"

static PROCESS_METER meter;
static CRITICAL_SECTION lock;
static int paused, status, rowCount, initialized;
static DWORD lastTick;
/* CPU totals include all threads, including threads which have already exited.
   Creation time distinguishes reused PIDs. Storage grows only at poll time. */
typedef struct
{
    DWORD pid;
    FILETIME created;
    double ticks;
} NT_PROCESS;
static NT_PROCESS *ntPrevious;
static DWORD ntCount;
static double ntTotal;
static void nt_reset(void)
{
    free(ntPrevious);
    ntPrevious = NULL;
    ntCount = 0;
    ntTotal = 0;
}
typedef struct
{
    char name[MAX_PATH];
    DWORD pid;
    double percent;
} PROCESS_ROW;
static PROCESS_ROW rows[5];

void processes_init(void)
{
    InitializeCriticalSection(&lock);
    meter.device = INVALID_HANDLE_VALUE;
    initialized = 1;
}

void processes_pause(int value)
{
    if (!initialized)
    {
        paused = value != 0;
        return;
    }
    EnterCriticalSection(&lock);
    paused = value != 0;
    if (paused)
    {
        process_meter_stop(&meter);
        nt_reset();
        lastTick = 0;
        rowCount = 0;
        status = 0;
    }
    LeaveCriticalSection(&lock);
}

void processes_stop(void)
{
    EnterCriticalSection(&lock);
    process_meter_stop(&meter);
    nt_reset();
    lastTick = 0;
    rowCount = 0;
    LeaveCriticalSection(&lock);
}

static void collect_rows(double elapsed)
{
    HANDLE snapshot;
    PROCESSENTRY32 pe;
    PROCESS_ROW row;
    DWORD i, delta;
    int j, k;
    char *name;
    rowCount = 0;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        status = 3;
        return;
    }
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32First(snapshot, &pe))
        do
        {
            for (i = 0; i < meter.current.count; i++)
                if (meter.current.processes[i].pid == pe.th32ProcessID && meter.current.processes[i].threads)
                {
                    delta = process_meter_delta(&meter, i);
                    if (delta > elapsed + 64.0)
                    {
                        status = 0;
                        rowCount = 0;
                        CloseHandle(snapshot);
                        return;
                    }
                    row.pid = pe.th32ProcessID;
                    row.percent = delta * 100.0 / elapsed;
                    if (row.percent > 100.0)
                        row.percent = 100.0;
                    name = strrchr(pe.szExeFile, '\\');
                    lstrcpynA(row.name, name ? name + 1 : pe.szExeFile, sizeof(row.name));
                    for (j = 0; j < rowCount; j++)
                        if (row.percent > rows[j].percent ||
                                (row.percent == rows[j].percent && row.pid < rows[j].pid))
                            break;
                    if (j < 5)
                    {
                        if (rowCount < 5)
                            rowCount++;
                        for (k = rowCount - 1; k > j; k--)
                            rows[k] = rows[k - 1];
                        rows[j] = row;
                    }
                    break;
                }
        }
        while (Process32Next(snapshot, &pe));
    CloseHandle(snapshot);
}

static void collect_nt(void)
{
    HANDLE snapshot, process;
    PROCESSENTRY32 pe;
    FILETIME created, exited, kernel, user;
    NT_PROCESS *current = NULL, *grown;
    DWORD count = 0, capacity = 0, i;
    double idle, total, elapsed, ticks, delta;
    PROCESS_ROW row;
    int j, k;
    char *name;
    rowCount = 0;
    if (!glass_system_times(&idle, &total))
    {
        status = 3;
        nt_reset();
        return;
    }
    elapsed = total - ntTotal;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        status = 3;
        nt_reset();
        return;
    }
    status = ntTotal && elapsed > 0 ? 5 : 0;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (Process32First(snapshot, &pe))
        do
        {
            if (!pe.th32ProcessID)
                continue; /* Idle is not an application. */
            process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            if (!process)
                continue;
            i = GetProcessTimes(process, &created, &exited, &kernel, &user);
            CloseHandle(process);
            if (!i)
                continue; /* Protected processes remain outside the ranking. */
            if (count == capacity)
            {
                capacity += 64;
                grown = realloc(current, capacity * sizeof(*current));
                if (!grown)
                {
                    free(current);
                    CloseHandle(snapshot);
                    nt_reset();
                    status = 3;
                    rowCount = 0;
                    return;
                }
                current = grown;
            }
            ticks = glass_filetime(kernel) + glass_filetime(user);
            current[count].pid = pe.th32ProcessID;
            current[count].created = created;
            current[count++].ticks = ticks;
            for (i = 0; i < ntCount; i++)
                if (ntPrevious[i].pid == pe.th32ProcessID &&
                        !CompareFileTime(&ntPrevious[i].created, &created))
                    break;
            if (i == ntCount || status != 5)
                continue;
            delta = ticks - ntPrevious[i].ticks;
            if (delta < 0)
                continue;
            row.pid = pe.th32ProcessID;
            row.percent = delta * 100.0 / elapsed;
            if (row.percent > 100.0)
                row.percent = 100.0;
            name = strrchr(pe.szExeFile, '\\');
            lstrcpynA(row.name, name ? name + 1 : pe.szExeFile, sizeof(row.name));
            for (j = 0; j < rowCount; j++)
                if (row.percent > rows[j].percent ||
                        (row.percent == rows[j].percent && row.pid < rows[j].pid))
                    break;
            if (j < 5)
            {
                if (rowCount < 5)
                    rowCount++;
                for (k = rowCount - 1; k > j; k--)
                    rows[k] = rows[k - 1];
                rows[j] = row;
            }
        }
        while (Process32Next(snapshot, &pe));
    CloseHandle(snapshot);
    free(ntPrevious);
    ntPrevious = current;
    ntCount = count;
    ntTotal = total;
}

void processes_update(const char *ini, int enabled, int seconds)
{
    char method[16];
    DWORD tick, error;
    double elapsed;
    int result;
    EnterCriticalSection(&lock);
    if (!enabled || paused)
    {
        process_meter_stop(&meter);
        nt_reset();
        lastTick = 0;
        rowCount = 0;
        status = 0;
        LeaveCriticalSection(&lock);
        return;
    }
    if (!(GetVersion() & 0x80000000UL))
    {
        tick = GetTickCount();
        if (!lastTick || tick - lastTick >= (DWORD)seconds * 1000)
        {
            lastTick = tick;
            collect_nt();
        }
        LeaveCriticalSection(&lock);
        return;
    }
    GetPrivateProfileStringA("CPU", "Method", "Legacy", method, sizeof(method), ini);
    if (lstrcmpiA(method, "VxD"))
    {
        process_meter_stop(&meter);
        nt_reset();
        status = 1;
        rowCount = 0;
        lastTick = 0;
        LeaveCriticalSection(&lock);
        return;
    }
    tick = GetTickCount();
    if (lastTick && tick - lastTick < (DWORD)seconds * 1000)
    {
        LeaveCriticalSection(&lock);
        return;
    }
    lastTick = tick;
    if (meter.device == INVALID_HANDLE_VALUE)
    {
        error = process_meter_start(&meter);
        if (error)
        {
            status = error == ERROR_NOT_SUPPORTED ? 2 : 3;
            rowCount = 0;
            LeaveCriticalSection(&lock);
            return;
        }
    }
    result = process_meter_read(&meter, &elapsed);
    if (result < 0)
    {
        status = meter.current.flags & G98PROC_OVERFLOW ? 4 : 3;
        rowCount = 0;
        process_meter_stop(&meter);
        nt_reset();
    }
    else if (!result)
    {
        status = 0;
        rowCount = 0;
    }
    else
    {
        status = 5;
        collect_rows(elapsed);
        meter.previous = meter.current;
    }
    LeaveCriticalSection(&lock);
}

void processes_write(FILE *file)
{
    int i, count, state;
    PROCESS_ROW copy[5];
    EnterCriticalSection(&lock);
    count = rowCount;
    state = status;
    memcpy(copy, rows, sizeof(copy));
    LeaveCriticalSection(&lock);
    fprintf(file, "var processStatus=%d;var processes=[", state);
    for (i = 0; i < count; i++)
    {
        if (i)
            fputc(',', file);
        fputs("{name:", file);
        js_string(file, copy[i].name);
        fprintf(file, ",pid:%lu,cpu:%.1f}", copy[i].pid, copy[i].percent);
    }
    fputs("];\n", file);
}
