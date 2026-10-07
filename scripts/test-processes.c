#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/vxd/g98cpu.h"
#include "../src/vxd/process-account.h"
#include "../src/bridge/procmeter.h"

int main(void)
{
    DWORD i, group, generation;
    G98PROC_THREAD *t;
    static PROCESS_METER meter;
    proc_reset();
    proc_start(0x1000, 1, 42, 100);
    group = proc_find(0x1000, 0)->group;
    /* Keep the main thread alive while 10,000 short-lived workers reuse one
       handle; all completed CPU time must survive between sample requests. */
    for (i = 1; i <= 10000; i++)
    {
        proc_start(0x2000, i, 42, 0);
        proc_exit(0x2000, i, 20);
    }
    assert(procGroups[group].milliseconds == 200000);
    assert(procGroups[group].threads == 1);
    assert(!procFlags);
    /* Delayed notifications for a reused handle must not remove its new owner. */
    proc_start(0x2000, 10001, 42, 0);
    proc_exit(0x2000, 10000, 900);
    assert(proc_find(0x2000, 0));
    proc_exit(0x2000, 10001, 50);
    assert(procGroups[group].milliseconds == 200050);
    /* Unsigned thread and aggregate counter rollover. */
    t = proc_find(0x1000, 0);
    t->last = 0xfffffff0UL;
    procGroups[group].milliseconds = 0xfffffff0UL;
    proc_update(t, 0x10);
    assert(procGroups[group].milliseconds == 0x10);
    generation = procGroups[group].generation;
    proc_exit(0x1000, 1, 0x10);
    proc_start(0x1000, 2, 42, 0);
    group = proc_find(0x1000, 0)->group;
    assert(procGroups[group].generation != generation);
    assert(procGroups[group].milliseconds == 0);
    /* Capacity must fail explicitly and cannot corrupt existing entries. */
    proc_reset();
    for (i = 0; i < G98PROC_THREADS; i++)
        proc_start(0x1000 + i * 16, i, 42, 0);
    assert(!procFlags);
    proc_start(0x100000, 1, 42, 0);
    assert(procFlags & G98PROC_OVERFLOW);
    assert(proc_find(0x1000, 0));
    proc_reset();
    for (i = 0; i < G98PROC_PROCESSES; i++)
        proc_start(0x1000 + i * 16, i, i + 1, 0);
    assert(!procFlags);
    proc_start(0x100000, 1, 999, 0);
    assert(procFlags & G98PROC_OVERFLOW);
    proc_reset();
    assert(!procFlags);
    /* Client deltas separate process generations, including reused PIDs. */
    meter.previous.count = meter.current.count = 1;
    meter.previous.processes[0].pid = meter.current.processes[0].pid = 42;
    meter.previous.processes[0].generation = meter.current.processes[0].generation = 1;
    meter.previous.processes[0].milliseconds = 0xfffffff0UL;
    meter.current.processes[0].milliseconds = 0x10;
    assert(process_meter_delta(&meter, 0) == 32);
    meter.current.processes[0].generation = 2;
    assert(process_meter_delta(&meter, 0) == 16);
    puts("Process accounting: short-lived threads, reuse, rollover and bounds passed.");
    return 0;
}
