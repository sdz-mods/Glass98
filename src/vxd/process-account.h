/* Bounded accounting shared by the driver and the host-side regression test.
   Call only while thread switching is excluded. No allocation or OS calls. */
typedef struct
{
    DWORD handle, serial, last, group;
} G98PROC_THREAD;
static G98PROC_THREAD procThreads[G98PROC_THREADS];
static G98PROC_PROCESS procGroups[G98PROC_PROCESSES];
static DWORD procFlags, procGeneration, procStarts, procExits;

static void proc_reset(void)
{
    DWORD i;
    for (i = 0; i < G98PROC_THREADS; i++)
        procThreads[i].handle = 0;
    for (i = 0; i < G98PROC_PROCESSES; i++)
        procGroups[i].pid = 0;
    procFlags = procStarts = procExits = 0;
}

static G98PROC_THREAD *proc_find(DWORD handle, int create)
{
    DWORD i, slot = ((handle >> 4) ^ (handle >> 13)) & (G98PROC_THREADS - 1);
    G98PROC_THREAD *t, *freeSlot = 0;
    for (i = 0; i < G98PROC_THREADS; i++)
    {
        t = &procThreads[(slot + i) & (G98PROC_THREADS - 1)];
        if (t->handle == handle)
            return t;
        if (t->handle <= 1 && !freeSlot)
            freeSlot = t;
        if (!t->handle)
            break;
    }
    if (!create)
        return 0;
    if (!freeSlot)
    {
        procFlags |= G98PROC_OVERFLOW;
        return 0;
    }
    freeSlot->handle = handle;
    freeSlot->group = G98PROC_PROCESSES;
    return freeSlot;
}

static DWORD proc_group(DWORD pid)
{
    DWORD i, freeSlot = G98PROC_PROCESSES;
    for (i = 0; i < G98PROC_PROCESSES; i++)
    {
        if (procGroups[i].pid == pid && procGroups[i].threads)
            return i;
        if ((!procGroups[i].pid || !procGroups[i].threads) && freeSlot == G98PROC_PROCESSES)
            freeSlot = i;
    }
    if (freeSlot == G98PROC_PROCESSES)
        procFlags |= G98PROC_OVERFLOW;
    else
    {
        if (!++procGeneration)
            ++procGeneration;
        procGroups[freeSlot].pid = pid;
        procGroups[freeSlot].generation = procGeneration;
        procGroups[freeSlot].milliseconds = 0;
        procGroups[freeSlot].threads = 0;
    }
    return freeSlot;
}

static void proc_assign(G98PROC_THREAD *t, DWORD pid)
{
    if (!pid || t->group < G98PROC_PROCESSES)
        return;
    t->group = proc_group(pid);
    if (t->group < G98PROC_PROCESSES)
        procGroups[t->group].threads++;
}

static void proc_start(DWORD handle, DWORD serial, DWORD pid, DWORD time)
{
    G98PROC_THREAD *t = proc_find(handle, 1);
    if (!t)
        return;
    /* A reused handle must not inherit the preceding thread's counter. */
    if (t->group < G98PROC_PROCESSES)
        procGroups[t->group].threads--;
    t->serial = serial;
    t->last = time;
    t->group = G98PROC_PROCESSES;
    proc_assign(t, pid);
}

static void proc_update(G98PROC_THREAD *t, DWORD time)
{
    if (t->group < G98PROC_PROCESSES)
        procGroups[t->group].milliseconds += time - t->last;
    t->last = time;
}

static void proc_exit(DWORD handle, DWORD serial, DWORD time)
{
    G98PROC_THREAD *t = proc_find(handle, 0);
    if (!t || t->serial != serial)
        return;
    proc_update(t, time);
    if (t->group < G98PROC_PROCESSES)
        procGroups[t->group].threads--;
    t->handle = 1; /* Tombstone preserves hash chains through this slot. */
}
