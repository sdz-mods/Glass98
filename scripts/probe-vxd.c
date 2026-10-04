/* Win98 diagnostic: bounded runs, balanced timer requests, raw counters. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/vxd/g98cpu.h"

static DWORD registry_cpu(const char *path)
{
    HKEY key;
    DWORD value = 0xffffffffUL, type, size = sizeof(value);
    if (RegOpenKeyExA(HKEY_DYN_DATA, path, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        RegQueryValueExA(key, "KERNEL\\CPUUsage", NULL, &type, (BYTE *)&value, &size);
        RegCloseKey(key);
    }
    return value;
}

int main(int argc, char **argv)
{
    HANDLE device;
    G98CPU_SAMPLE sample, previous;
    DWORD returned, i, start;
    unsigned period = 0;
    int busy = 0;
    char *end;
    long argument;
    double elapsed, usage;
    FILE *log;

    if (argc > 3)
    {
        printf("Usage: CPUCHECK [timer period 0..55 ms] [busy duty 0..100 percent]\n");
        return 1;
    }
    for (i = 1; i < (DWORD)argc; i++)
    {
        argument = strtol(argv[i], &end, 10);
        if (!argv[i][0] || *end || argument < 0 || argument > (i == 1 ? 55 : 100))
        {
            printf("Usage: CPUCHECK [timer period 0..55 ms] [busy duty 0..100 percent]\n");
            return 1;
        }
        if (i == 1)
            period = (unsigned)argument;
        else
            busy = (int)argument;
    }
    log = fopen("CPUVXD.LOG", "w");
    if (!log)
    {
        printf("Cannot create CPUVXD.LOG in the current directory.\n");
        return 1;
    }
    device = CreateFileA("\\\\.\\G98CPU", 0, 0, NULL, 0, 0x10000000UL, NULL);
    if (device == INVALID_HANDLE_VALUE)
    {
        fprintf(log, "Driver open error=%lu\n", GetLastError());
        printf("Driver unavailable. Run CPUSETUP install, then restart Windows.\n");
        fclose(log);
        return 2;
    }
    if (period && timeBeginPeriod(period) != TIMERR_NOERROR)
    {
        printf("The requested timer period is unavailable.\n");
        CloseHandle(device);
        fclose(log);
        return 3;
    }
    registry_cpu("PerfStats\\StartStat");
    printf("12-second test, timer request %u ms, busy duty %d%%. Log: CPUVXD.LOG\n", period, busy);
    fprintf(log, "period=%u busy=%d\n", period, busy);
    for (i = 0; i < 12; i++)
    {
        memset(&sample, 0, sizeof(sample));
        if (!DeviceIoControl(device, G98CPU_READ, NULL, 0, &sample, sizeof(sample), &returned, NULL))
        {
            fprintf(log, "Read error=%lu\n", GetLastError());
            break;
        }
        if (returned != sizeof(sample) || sample.size != sizeof(sample) || sample.version != G98CPU_VERSION)
        {
            fprintf(log, "Incompatible driver response\n");
            break;
        }
        fprintf(log, "%lu bytes=%lu version=%lu system=%lu systhread=%lu caller=%lu pit=%lu:%lu\n",
                GetTickCount(), returned, sample.version, sample.systemMs,
                sample.systemThreadMs, sample.callerThreadMs, sample.pitHigh, sample.pitLow);
        if (i)
        {
            elapsed = ((double)sample.pitHigh * 4294967296.0 + sample.pitLow -
                       ((double)previous.pitHigh * 4294967296.0 + previous.pitLow)) / 1193.182;
            if (elapsed <= 0.0 || elapsed > 60000.0)
            {
                fprintf(log, "Invalid elapsed time\n");
                break;
            }
            /* Unsigned subtraction handles the millisecond counter wrapping. */
            usage = 100.0 * (1.0 - (DWORD)(sample.systemThreadMs - previous.systemThreadMs) / elapsed);
            /* Preserve raw values in the log, including sub-ms sampling skew. */
            printf("CPU %.2f%%\n", usage < 0.0 ? 0.0 : usage > 100.0 ? 100.0 : usage);
            fprintf(log, "driver=%.2f registry=%lu elapsed=%.3f self=%lu\n", usage,
                    registry_cpu("PerfStats\\StatData"), elapsed,
                    (DWORD)(sample.callerThreadMs - previous.callerThreadMs));
        }
        previous = sample;
        fflush(log);
        start = GetTickCount();
        while (GetTickCount() - start < (DWORD)busy * 10) {}
        if (busy < 100)
            Sleep(1000 - busy * 10);
    }
    registry_cpu("PerfStats\\StopStat");
    if (period)
        timeEndPeriod(period);
    CloseHandle(device);
    fclose(log);
    return i == 12 ? 0 : 4;
}
