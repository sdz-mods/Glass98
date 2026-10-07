#ifndef GLASS98_PROCESS_METER_H
#define GLASS98_PROCESS_METER_H
#include <tlhelp32.h>
#include "../vxd/g98cpu.h"

typedef struct
{
    HANDLE device;
    int baseline;
    G98PROC_SAMPLE previous, current;
    G98PROC_BINDINGS bindings;
} PROCESS_METER;

static DWORD process_tdb(void);
#pragma aux process_tdb = "mov eax,fs:[18h]" "sub eax,8" value [eax] modify exact [eax];

static void process_meter_stop(PROCESS_METER *meter)
{
    DWORD returned;
    if (meter->device && meter->device != INVALID_HANDLE_VALUE)
    {
        DeviceIoControl(meter->device, G98PROC_STOP, NULL, 0, NULL, 0, &returned, NULL);
        CloseHandle(meter->device);
    }
    meter->device = INVALID_HANDLE_VALUE;
    meter->baseline = 0;
}

static DWORD process_meter_start(PROCESS_METER *meter)
{
    DWORD pid = GetCurrentProcessId(), returned, error = ERROR_INVALID_DATA;
    DWORD own, cookie, tdb, data[0x58 / 4], got, i;
    HANDLE snapshot;
    THREADENTRY32 te;
    OSVERSIONINFOA os;
    int ownMatched = 0;
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    if (!GetVersionExA(&os) || os.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS || os.dwMajorVersion != 4 ||
            os.dwMinorVersion != 10)
        return ERROR_NOT_SUPPORTED;
    meter->device = CreateFileA("\\\\.\\G98CPU", 0, 0, NULL, 0, 0x10000000UL, NULL);
    if (meter->device == INVALID_HANDLE_VALUE)
        return GetLastError();
    if (!DeviceIoControl(meter->device, G98PROC_BEGIN, &pid, sizeof(pid), &meter->bindings, sizeof(meter->bindings),
                         &returned, NULL))
    {
        error = GetLastError();
        goto failed;
    }
    if (returned != sizeof(meter->bindings) || meter->bindings.size != sizeof(meter->bindings) ||
            meter->bindings.version != G98PROC_VERSION || meter->bindings.count > G98PROC_THREADS || meter->bindings.flags)
        goto failed;
    /* Win98 ABI: TDB = FS:[18h]-8, self at 20h, owner at 38h, ring-0
       thread at 54h. ReadProcessMemory protects against concurrent exits.
       Validate all links; never ask the driver to dereference these addresses. */
    own = process_tdb();
    cookie = own ^ GetCurrentThreadId();
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        error = GetLastError();
        goto failed;
    }
    memset(&te, 0, sizeof(te));
    te.dwSize = sizeof(te);
    if (Thread32First(snapshot, &te))
        do
        {
            tdb = te.th32ThreadID ^ cookie;
            if (!ReadProcessMemory(GetCurrentProcess(), (void *)tdb, data, sizeof(data), &got) || got != sizeof(data) ||
                    data[0x20 / 4] != tdb + 8 || (data[0x38 / 4] ^ cookie) != te.th32OwnerProcessID)
                continue;
            for (i = 0; i < meter->bindings.count; i++)
                if (meter->bindings.threads[i].handle == data[0x54 / 4])
                {
                    meter->bindings.threads[i].pid = te.th32OwnerProcessID;
                    if (tdb == own)
                        ownMatched = 1;
                    break;
                }
        }
        while (Thread32Next(snapshot, &te));
    CloseHandle(snapshot);
    if (!ownMatched)
        goto failed;
    if (!DeviceIoControl(meter->device, G98PROC_BIND, &meter->bindings, sizeof(meter->bindings), NULL, 0, &returned, NULL))
    {
        error = GetLastError();
        goto failed;
    }
    meter->baseline = 0;
    return ERROR_SUCCESS;
failed:
    process_meter_stop(meter);
    return error;
}

/* Return 0 for a baseline, 1 for a valid interval, -1 for an invalid sample. */
static int process_meter_read(PROCESS_METER *meter, double *milliseconds)
{
    DWORD returned;
    if (!DeviceIoControl(meter->device, G98PROC_READ, NULL, 0, &meter->current, sizeof(meter->current), &returned, NULL) ||
            returned != sizeof(meter->current) || meter->current.size != sizeof(meter->current) ||
            meter->current.version != G98PROC_VERSION || meter->current.count > G98PROC_PROCESSES || meter->current.flags)
        return -1;
    *milliseconds = (((double)meter->current.pitHigh - meter->previous.pitHigh) * 4294967296.0 +
                     (double)meter->current.pitLow - meter->previous.pitLow) / 1193.182;
    if (!meter->baseline || *milliseconds < 100.0 || *milliseconds > 3605000.0)
    {
        meter->previous = meter->current;
        meter->baseline = 1;
        return 0;
    }
    return 1;
}

static DWORD process_meter_delta(const PROCESS_METER *meter, DWORD index)
{
    const G98PROC_PROCESS *process = &meter->current.processes[index];
    DWORD i;
    for (i = 0; i < meter->previous.count; i++)
        if (meter->previous.processes[i].pid == process->pid &&
                meter->previous.processes[i].generation == process->generation)
            return process->milliseconds - meter->previous.processes[i].milliseconds;
    /* A new generation began during this interval, so all its time belongs
       here. Reused process IDs never subtract from an old process counter. */
    return process->milliseconds;
}
#endif
