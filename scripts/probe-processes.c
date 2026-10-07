/* Bounded Win98 VM test for lifecycle accounting and pause/resume. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/bridge/procmeter.h"
static PROCESS_METER meter;
static G98CPU_SAMPLE cpuLast;
static char names[G98PROC_PROCESSES][MAX_PATH];

int main(int argc, char **argv)
{
    DWORD n, i, j, returned, result, delta, total, lastStarts = 0, lastExits = 0;
    DWORD period = argc > 1 ? atoi(argv[1]) : 5;
    DWORD samples = argc > 2 ? atoi(argv[2]) : 8;
    DWORD delay = argc > 3 ? atoi(argv[3]) : 2000;
    DWORD pauseAt = argc > 4 ? atoi(argv[4]) : 0;
    DWORD resumeAt = argc > 5 ? atoi(argv[5]) : 0;
    int quiet = argc > 6 ? atoi(argv[6]) : 0, paused = 0, valid, failures = 0;
    double elapsed, cost = 0, maxCost = 0, us, wall;
    LARGE_INTEGER rate, begin, end;
    HANDLE observer, snapshot;
    PROCESSENTRY32 pe;
    G98PROC_STATE state;
    G98CPU_SAMPLE cpu;
    if (period > 55 || samples < 2 || samples > 10000 || delay > 5000)
        return 1;
    observer = CreateFileA("\\\\.\\G98CPU", 0, 0, NULL, 0, 0x10000000UL, NULL);
    if (observer == INVALID_HANDLE_VALUE)
        return 2;
    if (period && timeBeginPeriod(period) != TIMERR_NOERROR)
        return 3;
    QueryPerformanceFrequency(&rate);
    result = process_meter_start(&meter);
    printf("start=%lu period=%lu\n", result, period);
    if (result)
        goto done;
    for (n = 0; n < samples; n++)
    {
        if (pauseAt && n == pauseAt)
        {
            process_meter_stop(&meter);
            paused = 1;
        }
        if (resumeAt && n == resumeAt)
        {
            result = process_meter_start(&meter);
            printf("resume=%lu\n", result);
            if (result)
                break;
            paused = 0;
        }
        QueryPerformanceCounter(&begin);
        if (!DeviceIoControl(observer, G98PROC_STATUS, NULL, 0, &state, sizeof(state), &returned, NULL))
        {
            result = GetLastError();
            break;
        }
        if (paused && (state.active || (n > pauseAt && (state.starts != lastStarts || state.exits != lastExits))))
            failures++;
        lastStarts = state.starts;
        lastExits = state.exits;
        if (!quiet)
            printf("sample=%lu active=%lu flags=%lu starts=%lu exits=%lu storage=%lu\n", n, state.active, state.flags, state.starts,
                   state.exits, state.bytes);
        if (!paused)
        {
            valid = process_meter_read(&meter, &elapsed);
            if (valid < 0)
            {
                result = 13;
                break;
            }
            if (!valid && !quiet)
                puts("baseline");
            if (valid)
            {
                memset(names, 0, sizeof(names));
                snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                pe.dwSize = sizeof(pe);
                if (snapshot != INVALID_HANDLE_VALUE)
                {
                    if (Process32First(snapshot, &pe))
                        do
                        {
                            for (i = 0; i < meter.current.count; i++)
                                if (meter.current.processes[i].pid == pe.th32ProcessID)
                                    lstrcpynA(names[i], pe.szExeFile, MAX_PATH);
                        }
                        while (Process32Next(snapshot, &pe));
                    CloseHandle(snapshot);
                }
                total = 0;
                for (i = 0; i < meter.current.count; i++)
                {
                    delta = process_meter_delta(&meter, i);
                    total += delta;
                    if (delta > elapsed + 64)
                        failures++;
                    if (delta && !quiet)
                        printf(" %.2f%% pid=%08lX gen=%lu threads=%lu %s\n", delta * 100.0 / elapsed, meter.current.processes[i].pid,
                               meter.current.processes[i].generation, meter.current.processes[i].threads,
                               names[i][0] ? names[i] : "[exited/unlisted]");
                }
                if (!quiet)
                    printf("attributed=%.2f%% elapsed=%.2f\n", total * 100.0 / elapsed, elapsed);
                meter.previous = meter.current;
            }
        }
        if (DeviceIoControl(observer, G98CPU_READ, NULL, 0, &cpu, sizeof(cpu), &returned, NULL))
        {
            wall = (((double)cpu.pitHigh - cpuLast.pitHigh) * 4294967296.0 + cpu.pitLow - (double)cpuLast.pitLow) / 1193.182;
            if (n && wall > 0 && !quiet)
                printf("system=%.2f%%\n", 100.0 - (cpu.systemThreadMs - cpuLast.systemThreadMs) * 100.0 / wall);
            cpuLast = cpu;
        }
        QueryPerformanceCounter(&end);
        us = (end.QuadPart - begin.QuadPart) * 1000000.0 / rate.QuadPart;
        cost += us;
        if (us > maxCost)
            maxCost = us;
        if (delay)
            Sleep(delay);
    }
    printf("wall_us mean=%.2f max=%.2f failures=%d\n", cost / samples, maxCost, failures);
done:
    process_meter_stop(&meter);
    /* Closing the owner must leave an independent observer able to verify idle. */
    if (DeviceIoControl(observer, G98PROC_STATUS, NULL, 0, &state, sizeof(state), &returned, NULL))
    {
        printf("after_close active=%lu\n", state.active);
        if (state.active)
            failures++;
    }
    CloseHandle(observer);
    if (period)
        timeEndPeriod(period);
    return result || failures ? 1 : 0;
}
