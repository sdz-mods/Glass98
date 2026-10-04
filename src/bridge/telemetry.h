#ifndef W98_TELEMETRY_H
#define W98_TELEMETRY_H
#include <windows.h>
#include "../vxd/g98cpu.h"
typedef struct
{
    BOOL started, sampled;
    BOOL useVxd, cpuBaseline;
    HANDLE cpuDevice;
    G98CPU_SAMPLE previousCpu;
    DWORD lastTick, startTick, counterType, counterSize;
    LONG cpu, lastError;
    MEMORYSTATUS memory;
    char vendor[13], brand[49];
    DWORD family, model, stepping, processors;
    DWORD frequencyMHz, frequencyKind; /* 1 base, 2 startup estimate, 3 TSC reference; 4-6 cached equivalents */
} TELEMETRY;
void telemetry_init(TELEMETRY *state);
void telemetry_sample(TELEMETRY *state);
void telemetry_stop(TELEMETRY *state);
void telemetry_text(TELEMETRY *state, char *text);
#endif
