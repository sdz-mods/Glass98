#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/bridge/telemetry.h"
#include "../src/bridge/cpucache.h"
int main(void)
{
    TELEMETRY s;
    char path[MAX_PATH], ini[MAX_PATH];
    assert(GetCurrentDirectoryA(sizeof(path), path));
    strcat(path, "\\cpu-cache-test");
    CreateDirectoryA(path, NULL);
    sprintf(ini, "%s\\WIDGETS.INI", path);
    DeleteFileA(ini);
    memset(&s, 0, sizeof(s));
    strcpy(s.vendor, "GenuineIntel");
    strcpy(s.brand, "Intel Pentium II");
    s.family = 6; s.model = 3; s.processors = 1;
    cpu_frequency_cache(&s, path);
    assert(!s.frequencyMHz);
    s.frequencyMHz = 300; s.frequencyKind = 2;
    cpu_frequency_cache(&s, path);
    s.frequencyMHz = s.frequencyKind = 0;
    cpu_frequency_cache(&s, path);
    assert(s.frequencyMHz == 300 && s.frequencyKind == 5);
    s.frequencyMHz = 333; s.frequencyKind = 2;
    cpu_frequency_cache(&s, path);
    s.frequencyMHz = s.frequencyKind = 0;
    cpu_frequency_cache(&s, path);
    assert(s.frequencyMHz == 333 && s.frequencyKind == 5);
    s.model = 5; s.frequencyMHz = s.frequencyKind = 0;
    cpu_frequency_cache(&s, path);
    assert(!s.frequencyMHz);
    s.model = 3;
    WritePrivateProfileStringA("CPUFrequency", "Reading", "GenuineIntel|Intel Pentium II|6|3|0|1|999999|2", ini);
    cpu_frequency_cache(&s, path);
    assert(!s.frequencyMHz);
    DeleteFileA(ini);
    RemoveDirectoryA(path);
    puts("PASS: CPU cache fallback, fresh replacement, mismatched CPU and malformed frequency");
    return 0;
}
