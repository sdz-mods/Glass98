#ifndef GLASS98_CPU_DRIVER_H
#define GLASS98_CPU_DRIVER_H
#define G98CPU_READ 1UL
#define G98CPU_VERSION 1UL
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
