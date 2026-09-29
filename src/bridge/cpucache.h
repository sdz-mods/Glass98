/* Persist only validated frequency readings, keyed by the detected CPU identity. */
#ifndef GLASS98_CPUCACHE_H
#define GLASS98_CPUCACHE_H
static void cpu_frequency_cache(TELEMETRY *state, const char *directory)
{
    char path[MAX_PATH], identity[200], record[256], stored[256], tail;
    unsigned long mhz, kind;
    size_t length;
    if (strlen(directory) + 13 >= MAX_PATH || !state->vendor[0] || !state->brand[0])
        return;
    sprintf(path, "%s\\WIDGETS.INI", directory);
    sprintf(identity, "%s|%s|%lu|%lu|%lu|%lu|", state->vendor, state->brand,
            state->family, state->model, state->stepping, state->processors);
    GetPrivateProfileStringA("CPUFrequency", "Reading", "", stored, sizeof(stored), path);
    if (state->frequencyMHz >= 10 && state->frequencyMHz <= 20000 &&
            state->frequencyKind >= 1 && state->frequencyKind <= 3)
    {
        sprintf(record, "%s%lu|%lu", identity, state->frequencyMHz, state->frequencyKind);
        if (strcmp(record, stored))
        {
            WritePrivateProfileStringA("CPUFrequency", "Reading", record, path);
            WritePrivateProfileStringA(NULL, NULL, NULL, path);
        }
        return;
    }
    length = strlen(identity);
    if (!strncmp(stored, identity, length) &&
            sscanf(stored + length, "%lu|%lu%c", &mhz, &kind, &tail) == 2 &&
            mhz >= 10 && mhz <= 20000 && kind >= 1 && kind <= 3)
    {
        state->frequencyMHz = mhz;
        state->frequencyKind = kind + 3; /* Cached base/startup/TSC reading. */
    }
}
#endif
