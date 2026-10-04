#ifndef GLASS98_CPU_METER_H
#define GLASS98_CPU_METER_H

/* Independent of wall-clock adjustments and multimedia timer resolution.
   Return -1 to rebaseline after a long gap, reset or invalid counter response. */
static LONG cpu_meter(const G98CPU_SAMPLE *previous, const G98CPU_SAMPLE *current)
{
    double elapsed, idle, busy;
    if (current->size != sizeof(*current) || current->version != G98CPU_VERSION ||
        previous->size != sizeof(*previous) || previous->version != G98CPU_VERSION)
        return -1;
    elapsed = (((double)current->pitHigh - previous->pitHigh) * 4294967296.0 +
               (double)current->pitLow - previous->pitLow) / 1193.182;
    idle = (DWORD)(current->systemThreadMs - previous->systemThreadMs);
    /* Sequential reads and millisecond rounding allow a small idle-time skew. */
    if (elapsed < 100.0 || elapsed > 60000.0 || idle > elapsed + 32.0)
        return -1;
    busy = 100.0 * (1.0 - idle / elapsed);
    return busy < 0.0 ? 0 : busy > 100.0 ? 100 : (LONG)(busy + 0.5);
}
#endif
