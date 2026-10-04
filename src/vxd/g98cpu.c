/* Read-only Win98 scheduler counters. No hooks, timer requests or polling. */
#ifndef VXD32
#error Build this file as a VxD.
#endif
#define WIN40SERVICES

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef int BOOL;
#define NULL 0
#include "include/vmm.h"
#include "g98cpu.h"

void __declspec(naked) VXD_control(void);

/* Watcom's LE loader glue requires the DDB first in initialized data. */
DDB VXD_DDB = {
    /* Opened by name only; no exported services require a numeric device ID.
       An arbitrary fixed ID can prevent loading alongside unrelated VxDs. */
    0, DDK_VERSION, UNDEFINED_DEVICE_ID, 0, 1, 0,
    {'G', '9', '8', 'C', 'P', 'U', ' ', ' '},
    VDD_Init_Order - 1, (DWORD)VXD_control,
    0, 0, 0, 0, 0, 0, 0, 0, 'Prev', sizeof(DDB), 'Rsv1', 'Rsv2', 'Rsv3'
};

static DWORD system_thread(void)
{
    DWORD result;
    _asm push edi
    VMMCall(Get_Sys_Thread_Handle);
    _asm mov result, edi
    _asm pop edi
    return result;
}

static DWORD current_thread(void)
{
    DWORD result;
    _asm push edi
    VMMCall(Get_Cur_Thread_Handle);
    _asm mov result, edi
    _asm pop edi
    return result;
}

static DWORD thread_time(DWORD thread)
{
    /* VMM execution accounting, in milliseconds; not KERNEL\CPUUsage. */
    DWORD result;
    _asm push thread
    VMMCall(_GetThreadExecTime);
    _asm add esp, 4
    _asm mov result, eax
    return result;
}

static DWORD system_time(void)
{
    DWORD result;
    VMMCall(Get_System_Time);
    _asm mov result, eax
    return result;
}

#define VTD__VTD_Get_Real_Time 7
static void real_time(DWORD *low, DWORD *high)
{
    /* VTD returns a 64-bit count at the PIT base rate (1193182 Hz). */
    DWORD lo, hi;
    VxDCall(VTD, VTD_Get_Real_Time);
    _asm mov lo, eax
    _asm mov hi, edx
    *low = lo;
    *high = hi;
}

static DWORD __stdcall cpu_ioctl(struct DIOCParams *params)
{
    G98CPU_SAMPLE *out;
    DWORD lo, hi;
    if (params->dwIoControlCode == DIOC_OPEN || params->dwIoControlCode == (DWORD)DIOC_CLOSEHANDLE)
        return 0;
    if (params->dwIoControlCode != G98CPU_READ)
        return 50; /* ERROR_NOT_SUPPORTED */
    if (!params->lpOutBuffer || params->cbOutBuffer < sizeof(G98CPU_SAMPLE) || !params->lpcbBytesReturned)
        return 87; /* ERROR_INVALID_PARAMETER */
    out = (G98CPU_SAMPLE *)params->lpOutBuffer;
    *(DWORD *)params->lpcbBytesReturned = 0;
    out->size = sizeof(*out);
    out->version = G98CPU_VERSION;
    out->systemMs = system_time();
    out->systemThreadMs = thread_time(system_thread());
    out->callerThreadMs = thread_time(current_thread());
    real_time(&lo, &hi);
    out->pitLow = lo;
    out->pitHigh = hi;
    *(DWORD *)params->lpcbBytesReturned = sizeof(*out);
    return 0;
}

void __declspec(naked) VXD_control(void)
{
    _asm {
        cmp eax, W32_DEVICEIOCONTROL
        jne control_ok
        pushad
        push esi
        call cpu_ioctl
        mov [esp + 28], eax
        popad
        clc
        ret
    control_ok:
        xor eax, eax
        clc
        ret
    }
}
