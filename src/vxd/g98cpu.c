/* Win98 scheduler counters and opt-in thread lifecycle accounting.
   No scheduler hooks, timer requests or background polling. */
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
DDB VXD_DDB =
{
    /* Opened by name only; no exported services require a numeric device ID.
       An arbitrary fixed ID can prevent loading alongside unrelated VxDs. */
    0, DDK_VERSION, UNDEFINED_DEVICE_ID, 0, 1, 0,
    {'G', '9', '8', 'C', 'P', 'U', ' ', ' '},
    VDD_Init_Order - 1, (DWORD)VXD_control,
    0, 0, 0, 0, 0, 0, 0, 0, 'Prev', sizeof(DDB), 'Rsv1', 'Rsv2', 'Rsv3'
};

#include "process-account.h"
static DWORD procActive, procOwner, procOwnerProcess, procCookie, procBusy;
static BYTE procSeen[G98PROC_THREADS];
/* Locked driver storage keeps pageable client buffers out of IRQ-off code. */
static union
{
    G98PROC_BINDINGS bindings;
    G98PROC_SAMPLE sample;
    G98PROC_STATE state;
} procBuffer;

static DWORD irq_save(void);
#pragma aux irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
static void irq_restore(DWORD flags);
#pragma aux irq_restore = "push eax" "popfd" parm [eax] modify exact [];

#define VWIN32__VWIN32_GetCurrentProcessHandle 13
static DWORD current_process(void)
{
    DWORD result;
    VxDCall(VWIN32, VWIN32_GetCurrentProcessHandle);
    _asm mov result, eax
    return result;
}

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

static DWORD next_thread(DWORD thread)
{
    DWORD result;
    _asm push edi
    _asm mov edi, thread
    VMMCall(Get_Next_Thread_Handle);
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

static void __stdcall process_event(DWORD event, DWORD thread)
{
    DWORD flags = irq_save();
    if (procActive)
    {
        if (event == Thread_Init)
        {
            /* Thread_Init executes in the new thread's process. Exit callbacks
               need not do so: use the ownership saved here on termination. */
            if (thread == current_thread())
            {
                proc_start(thread, *(WORD *)(thread + 0x18),
                           current_process() ^ procCookie, thread_time(thread));
                procStarts++;
            }
            else
                procFlags |= G98PROC_OVERFLOW;
        }
        else
        {
            proc_exit(thread, *(WORD *)(thread + 0x18), thread_time(thread));
            procExits++;
        }
    }
    irq_restore(flags);
}

static DWORD process_ioctl(struct DIOCParams *params)
{
    DWORD flags, code = params->dwIoControlCode, size, i, thread, first, pid = 0;
    G98PROC_THREAD *t;
    DWORD *src, *dst;
    if (code == G98PROC_STOP)
    {
        flags = irq_save();
        if (procOwner == params->hDevice && procOwnerProcess == params->tagProcess)
        {
            procActive = 0;
            procOwner = 0;
        }
        irq_restore(flags);
        return 0;
    }
    if (code < G98PROC_BEGIN || code > G98PROC_STATUS)
        return 50;
    size = code == G98PROC_BEGIN ? sizeof(G98PROC_BINDINGS) :
           code == G98PROC_READ ? sizeof(G98PROC_SAMPLE) : sizeof(G98PROC_STATE);
    if (code != G98PROC_BIND && (!params->lpOutBuffer || params->cbOutBuffer < size || !params->lpcbBytesReturned))
        return 87;
    if ((code == G98PROC_BEGIN && (!params->lpInBuffer || params->cbInBuffer != sizeof(DWORD))) ||
            (code == G98PROC_BIND && (!params->lpInBuffer || params->cbInBuffer != sizeof(G98PROC_BINDINGS))))
        return 87;
    flags = irq_save();
    if (procBusy || (code != G98PROC_STATUS && procOwner &&
                     (procOwner != params->hDevice || procOwnerProcess != params->tagProcess)))
    {
        irq_restore(flags);
        return 170; /* ERROR_BUSY: one accounting client, independent CPU readers. */
    }
    procBusy = 1;
    irq_restore(flags);
    /* Marshal with interrupts enabled; these user pages may fault. The busy
       guard prevents another IOCTL from overwriting the scratch buffer. */
    if (code == G98PROC_BEGIN)
        pid = *(DWORD *)params->lpInBuffer;
    if (code == G98PROC_BIND)
    {
        src = (DWORD *)params->lpInBuffer;
        dst = (DWORD *)&procBuffer.bindings;
        for (i = 0; i < sizeof(G98PROC_BINDINGS) / 4; i++)
            dst[i] = src[i];
        if (procBuffer.bindings.size != sizeof(G98PROC_BINDINGS) ||
                procBuffer.bindings.version != G98PROC_VERSION || procBuffer.bindings.count > G98PROC_THREADS)
        {
            procBusy = 0;
            return 87;
        }
    }
    flags = irq_save();
    if (code == G98PROC_BEGIN)
    {
        procActive = 0;
        proc_reset();
        procCookie = current_process() ^ pid;
        procOwner = params->hDevice;
        procOwnerProcess = params->tagProcess;
        procBuffer.bindings.size = sizeof(G98PROC_BINDINGS);
        procBuffer.bindings.version = G98PROC_VERSION;
        procBuffer.bindings.count = 0;
        first = thread = system_thread();
        do
        {
            if (procBuffer.bindings.count == G98PROC_THREADS)
            {
                procFlags |= G98PROC_OVERFLOW;
                break;
            }
            if (thread != first)
            {
                i = procBuffer.bindings.count++;
                procBuffer.bindings.threads[i].handle = thread;
                procBuffer.bindings.threads[i].serial = *(WORD *)(thread + 0x18);
                procBuffer.bindings.threads[i].pid = 0;
                proc_start(thread, *(WORD *)(thread + 0x18), 0, thread_time(thread));
            }
            thread = next_thread(thread);
        }
        while (thread && thread != first);
        procBuffer.bindings.flags = procFlags;
        procActive = 1;
    }
    else if (code == G98PROC_BIND && procActive && procOwner == params->hDevice)
    {
        for (i = 0; i < procBuffer.bindings.count; i++)
        {
            G98PROC_MAP *map = &procBuffer.bindings.threads[i];
            t = map->handle > 1 ? proc_find(map->handle, 0) : 0;
            if (t && t->serial == map->serial)
                proc_assign(t, map->pid);
        }
    }
    else if (code == G98PROC_READ)
    {
        procBuffer.sample.size = sizeof(G98PROC_SAMPLE);
        procBuffer.sample.version = G98PROC_VERSION;
        procBuffer.sample.count = 0;
        if (procActive && procOwner == params->hDevice)
        {
            /* Walk VMM's live list rather than dereferencing saved handles:
               a terminated thread may already have lost its execution state. */
            for (i = 0; i < G98PROC_THREADS; i++)
                procSeen[i] = 0;
            first = thread = system_thread();
            do
            {
                t = proc_find(thread, 0);
                if (t && t->serial == *(WORD *)(thread + 0x18))
                {
                    procSeen[t - procThreads] = 1;
                    proc_update(t, thread_time(thread));
                }
                thread = next_thread(thread);
            }
            while (thread && thread != first);
            for (i = 0; i < G98PROC_THREADS; i++)
            {
                t = &procThreads[i];
                if (t->handle > 1 && !procSeen[i])
                {
                    /* Never retain a stale kernel handle or silently rank an
                       interval whose final execution count was not delivered. */
                    if (t->group < G98PROC_PROCESSES)
                        procFlags |= G98PROC_INCOMPLETE;
                    proc_exit(t->handle, t->serial, t->last);
                }
            }
            for (i = 0; i < G98PROC_PROCESSES; i++)
                if (procGroups[i].pid)
                    procBuffer.sample.processes[procBuffer.sample.count++] = procGroups[i];
        }
        procBuffer.sample.flags = procFlags | (procActive ? 0 : G98PROC_INACTIVE);
        procBuffer.sample.starts = procStarts;
        procBuffer.sample.exits = procExits;
        real_time(&procBuffer.sample.pitLow, &procBuffer.sample.pitHigh);
    }
    else if (code == G98PROC_STATUS)
    {
        procBuffer.state.size = sizeof(G98PROC_STATE);
        procBuffer.state.version = G98PROC_VERSION;
        procBuffer.state.active = procActive;
        procBuffer.state.flags = procFlags;
        procBuffer.state.starts = procStarts;
        procBuffer.state.exits = procExits;
        procBuffer.state.bytes = sizeof(procThreads) + sizeof(procGroups) + sizeof(procBuffer) + sizeof(procSeen);
    }
    irq_restore(flags);
    if (code != G98PROC_BIND)
    {
        src = (DWORD *)&procBuffer;
        dst = (DWORD *)params->lpOutBuffer;
        for (i = 0; i < size / 4; i++)
            dst[i] = src[i];
        *(DWORD *)params->lpcbBytesReturned = size;
    }
    procBusy = 0;
    return 0;
}

static DWORD __stdcall cpu_ioctl(struct DIOCParams *params)
{
    G98CPU_SAMPLE *out;
    DWORD lo, hi;
    if (params->dwIoControlCode == (DWORD)DIOC_CLOSEHANDLE)
    {
        if (procOwner == params->hDevice && procOwnerProcess == params->tagProcess)
        {
            procActive = 0;
            procOwner = 0;
        }
        return 0;
    }
    if (params->dwIoControlCode == DIOC_OPEN)
        return 0;
    if (params->dwIoControlCode != G98CPU_READ)
        return process_ioctl(params);
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
    _asm
    {
        cmp eax, Thread_Init
        je thread_notification
        cmp eax, Thread_Not_Executeable
        je thread_notification
        cmp eax, W32_DEVICEIOCONTROL
        jne control_ok
        pushad
        push esi
        call cpu_ioctl
        mov [esp + 28], eax
        popad
        clc
        ret
        thread_notification:
        cmp procActive, 0
        je control_ok
        pushad
        push edi
        push eax
        call process_event
        popad
        control_ok:
        xor eax, eax
        clc
        ret
    }
}
