#ifndef GLASS98_CPU_DRIVER_H
#define GLASS98_CPU_DRIVER_H
#define G98CPU_READ 1UL
#define G98CPU_VERSION 1UL
#define G98PROC_BEGIN 2UL
#define G98PROC_BIND 3UL
#define G98PROC_READ 4UL
#define G98PROC_STOP 5UL
#define G98PROC_STATUS 6UL
#define G98PROC_VERSION 1UL
#define G98PROC_THREADS 512
#define G98PROC_PROCESSES 128
#define G98PROC_OVERFLOW 1UL
#define G98PROC_INACTIVE 2
#define G98PROC_INCOMPLETE 4UL
typedef struct
{
    DWORD handle, serial, pid;
} G98PROC_MAP;
typedef struct
{
    DWORD size, version, count, flags;
    G98PROC_MAP threads[G98PROC_THREADS];
} G98PROC_BINDINGS;
typedef struct
{
    DWORD pid, generation, milliseconds, threads;
} G98PROC_PROCESS;
typedef struct
{
    DWORD size, version, count, flags, pitLow, pitHigh, starts, exits;
    G98PROC_PROCESS processes[G98PROC_PROCESSES];
} G98PROC_SAMPLE;
typedef struct
{
    DWORD size, version, active, flags, starts, exits, bytes;
} G98PROC_STATE;
/* DWORD must be a 32-bit unsigned integer in both the VxD and Win32 client. */
typedef struct
{
    DWORD size;
    DWORD version;
    DWORD systemMs;
    DWORD systemThreadMs;
    DWORD callerThreadMs;
    DWORD pitLow;
    DWORD pitHigh;
} G98CPU_SAMPLE;
#endif
