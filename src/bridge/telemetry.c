#include "telemetry.h"
#include <stdio.h>
#include <string.h>
#include "cpuname.h"
#include "cpufreq.h"
#include "cpumeter.h"
#include <excpt.h>

static int has_cpuid(void);
#pragma aux has_cpuid = \
    "pushfd" "pop eax" "mov ecx,eax" "xor eax,200000h" \
    "push eax" "popfd" "pushfd" "pop eax" "push ecx" "popfd" \
    "xor eax,ecx" "and eax,200000h" value [eax] modify exact [eax ecx];
static void cpu_id(DWORD leaf, DWORD *output);
#pragma aux cpu_id = \
    "xor ecx,ecx" "db 0fh,0a2h" "mov [edi],eax" "mov [edi+4],ebx" \
    "mov [edi+8],ecx" "mov [edi+12],edx" \
    parm [eax] [edi] modify exact [eax ebx ecx edx];

static void read_tsc(DWORD *output);
#pragma aux read_tsc = \
    "xor eax,eax" "db 0fh,0a2h" "db 0fh,031h" \
    "mov [edi],eax" "mov [edi+4],edx" \
    parm [edi] modify exact [eax ebx ecx edx];
static double ticks64(DWORD low, DWORD high)
{
    return (double)high * 4294967296.0 + low;
}
static DWORD measured_frequency(void)
{
    LARGE_INTEGER rate, start, end;
    DWORD a[2], b[2], tick, elapsed;
    double samples[3], seconds, hz;
    int i;
    if (!QueryPerformanceFrequency(&rate))
        return 0;
    hz = ticks64(rate.LowPart, rate.HighPart);
    if (hz <= 0)
        return 0;
    __try
    {
        for (i = 0; i < 3; i++)
        {
            tick = GetTickCount();
            if (!QueryPerformanceCounter(&start))
                return 0;
            read_tsc(a);
            Sleep(250);
            read_tsc(b);
            if (!QueryPerformanceCounter(&end))
                return 0;
            elapsed = GetTickCount() - tick;
            seconds = (ticks64(end.LowPart, end.HighPart) - ticks64(start.LowPart, start.HighPart)) / hz;
            if (seconds < 0.2 || seconds > 2 || elapsed < 150 || elapsed > 2100 || seconds * 1000 < elapsed - 100.0 ||
                    seconds * 1000 > elapsed + 100.0)
                return 0;
            samples[i] = (ticks64(b[0], b[1]) - ticks64(a[0], a[1])) / seconds / 1000000.0;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
    return frequency_result(samples[0], samples[1], samples[2]);
}
static void cpu_frequency(TELEMETRY *s, DWORD maxLeaf, DWORD features, DWORD extMax)
{
    static int attempted = 0;
    static DWORD mhz = 0, kind = 0;
    DWORD words[4];
    int invariant = 0, legacy;
    if (!attempted)
    {
        attempted = 1;
        if (!strcmp(s->vendor, "GenuineIntel") && maxLeaf >= 0x16)
        {
            cpu_id(0x16, words);
            mhz = words[0] & 65535;
            if (mhz >= 10 && mhz <= 20000)
                kind = 1;
            else
                mhz = 0;
        }
        if (!mhz && (features & 16))
        {
            if (extMax >= 0x80000007UL)
            {
                cpu_id(0x80000007UL, words);
                invariant = (words[3] & 256) != 0;
            }
            legacy = (!strcmp(s->vendor, "GenuineIntel") && (s->family == 5 || (s->family == 6 && s->model <= 13))) ||
                     (!strcmp(s->vendor, "AuthenticAMD") && (s->family == 5 || s->family == 6));
            mhz = measured_frequency();
            if (mhz)
                kind = legacy && !invariant ? 2 : 3;
        }
    }
    s->frequencyMHz = mhz;
    s->frequencyKind = kind;
}

static LONG counter(const char *key, DWORD *value, DWORD *type, DWORD *size)
{
    HKEY handle;
    LONG error;
    *size = sizeof(DWORD);
    *value = 0;
    *type = 0;
    error = RegOpenKeyExA(HKEY_DYN_DATA, key, 0, KEY_READ, &handle);
    if (error != ERROR_SUCCESS)
        return error;
    error = RegQueryValueExA(handle, "KERNEL\\CPUUsage", NULL, type, (BYTE*)value, size);
    RegCloseKey(handle);
    return error;
}
static int intel_l2(void)
{
    DWORD words[4], rounds, r, j, k, descriptor;
    int size = -1;
    cpu_id(2, words);
    rounds = words[0] & 255;
    if (!rounds || rounds > 8)
        return -1;
    for (r = 0; r < rounds; r++)
    {
        if (r)
            cpu_id(2, words);
        for (j = 0; j < 4; j++)
            if (!(words[j] & 0x80000000UL))
                for (k = j ? 0 : 1; k < 4; k++)
                {
                    descriptor = (words[j] >> (k * 8)) & 255;
                    if (descriptor == 0x40 && size < 0)
                        size = 0;
                    if (descriptor >= 0x41 && descriptor <= 0x45)
                        size = 128 << (descriptor - 0x41);
                    if (descriptor >= 0x82 && descriptor <= 0x85)
                        size = 256 << (descriptor - 0x82);
                }
    }
    return size;
}

static BOOL use_cpu_driver(void)
{
    char path[MAX_PATH], method[16], *name;
    DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!length || length >= sizeof(path) || !(name = strrchr(path, '\\')) ||
        (size_t)(name + 1 - path) + sizeof("WIDGETS.INI") > sizeof(path))
        return FALSE;
    strcpy(name + 1, "WIDGETS.INI");
    GetPrivateProfileStringA("CPU", "Method", "Legacy", method, sizeof(method), path);
    return !lstrcmpiA(method, "VxD");
}

static BOOL read_cpu_driver(TELEMETRY *s, G98CPU_SAMPLE *value)
{
    DWORD returned;
    if (!DeviceIoControl(s->cpuDevice, G98CPU_READ, NULL, 0, value, sizeof(*value), &returned, NULL))
    {
        s->lastError = GetLastError();
        return FALSE;
    }
    if (returned != sizeof(*value) || value->size != sizeof(*value) || value->version != G98CPU_VERSION)
    {
        s->lastError = ERROR_INVALID_DATA;
        return FALSE;
    }
    s->lastError = ERROR_SUCCESS;
    return TRUE;
}

void telemetry_init(TELEMETRY *s)
{
    SYSTEM_INFO info;
    DWORD words[4], maxLeaf, extMax, features = 0, signature, baseFamily, baseModel, i, value, type, size, brand;
    char *p, brandText[49];
    const char *legacy;
    memset(s, 0, sizeof(*s));
    s->cpuDevice = INVALID_HANDLE_VALUE;
    s->cpu = -1;
    strcpy(s->brand, "Unknown processor");
    strcpy(s->vendor, "N/A");
    GetSystemInfo(&info);
    s->processors = info.dwNumberOfProcessors;
    if (has_cpuid())
    {
        cpu_id(0, words);
        maxLeaf = words[0];
        memcpy(s->vendor, words + 1, 4);
        memcpy(s->vendor + 4, words + 3, 4);
        memcpy(s->vendor + 8, words + 2, 4);
        s->vendor[12] = 0;
        if (maxLeaf >= 1)
        {
            cpu_id(1, words);
            features = words[3];
            signature = words[0];
            brand = words[1] & 255;
            baseFamily = (signature >> 8) & 15;
            baseModel = (signature >> 4) & 15;
            s->family = baseFamily + (baseFamily == 15 ? (signature >> 20) & 255 : 0);
            s->model = baseModel + ((baseFamily == 6 || baseFamily == 15) ? ((signature >> 16) & 15) * 16 : 0);
            s->stepping = signature & 15;
            sprintf(s->brand, "%s family %lu model %lu", s->vendor, s->family, s->model);
            legacy = legacy_cpu_name(s->vendor, s->family, s->model);
            if (legacy)
                lstrcpynA(s->brand, legacy, sizeof(s->brand));
            if (!strcmp(s->vendor, "GenuineIntel"))
            {
                legacy = intel_legacy_detail(signature, brand, maxLeaf >= 2 ? intel_l2() : -1);
                if (legacy)
                    lstrcpynA(s->brand, legacy, sizeof(s->brand));
            }
        }
        cpu_id(0x80000000UL, words);
        extMax = words[0];
        if (extMax >= 0x80000006UL && !strcmp(s->vendor, "AuthenticAMD"))
        {
            cpu_id(0x80000006UL, words);
            legacy = amd_legacy_detail(s->family, s->model, words[2] >> 16);
            if (legacy)
                lstrcpynA(s->brand, legacy, sizeof(s->brand));
        }
        if (extMax >= 0x80000004UL)
        {
            for (i = 0; i < 3; i++)
            {
                cpu_id(0x80000002UL + i, words);
                memcpy(brandText + i * 16, words, 16);
            }
            brandText[48] = 0;
            p = brandText;
            while (*p == ' ')
                p++;
            i = strlen(p);
            while (i && p[i - 1] == ' ')
                p[--i] = 0;
            if (*p)
                lstrcpynA(s->brand, p, sizeof(s->brand));
        }
        cpu_frequency(s, maxLeaf, features, extMax);
    }
    /* Delimiters/control characters must never enter the scripting wire format. */
    for (p = s->brand; *p; p++)
        if ((unsigned char)*p < 32 || *p == '|')
            *p = ' ';
    for (p = s->vendor; *p; p++)
        if ((unsigned char)*p < 32 || *p == '|')
            *p = ' ';
    if (!(GetVersion() & 0x80000000UL))
    {
        s->lastError = ERROR_NOT_SUPPORTED;
        return;
    }
    s->useVxd = use_cpu_driver();
    if (s->useVxd)
    {
        s->cpuDevice = CreateFileA("\\\\.\\G98CPU", 0, 0, NULL, 0, 0x10000000UL, NULL);
        if (s->cpuDevice == INVALID_HANDLE_VALUE)
            s->lastError = GetLastError();
        else
            s->cpuBaseline = read_cpu_driver(s, &s->previousCpu);
        /* Do not silently substitute the inaccurate counter if VxD was selected. */
        return;
    }
    s->lastError = counter("PerfStats\\StartStat", &value, &type, &size);
    s->started = (s->lastError == ERROR_SUCCESS);
    s->startTick = GetTickCount();
}
void telemetry_sample(TELEMETRY *s)
{
    DWORD tick = GetTickCount(), value;
    G98CPU_SAMPLE current;
    if (s->sampled && tick - s->lastTick < 900)
        return;
    s->lastTick = tick;
    s->sampled = TRUE;
    s->memory.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus(&s->memory);
    s->cpu = -1;
    if (s->useVxd)
    {
        if (s->cpuDevice != INVALID_HANDLE_VALUE && read_cpu_driver(s, &current))
        {
            if (s->cpuBaseline)
                s->cpu = cpu_meter(&s->previousCpu, &current);
            s->previousCpu = current;
            s->cpuBaseline = TRUE;
        }
        else
            s->cpuBaseline = FALSE;
        return;
    }
    if (s->started && tick - s->startTick >= 1000)
    {
        s->lastError = counter("PerfStats\\StatData", &value, &s->counterType, &s->counterSize);
        if (s->lastError == ERROR_SUCCESS && s->counterSize == 4 &&
                (s->counterType == REG_DWORD || s->counterType == REG_BINARY) && value <= 100)
            s->cpu = (LONG)value;
        else if (s->lastError == ERROR_SUCCESS)
            s->lastError = ERROR_INVALID_DATA;
    }
}
void telemetry_stop(TELEMETRY *s)
{
    DWORD value, type, size;
    if (s->cpuDevice && s->cpuDevice != INVALID_HANDLE_VALUE)
    {
        CloseHandle(s->cpuDevice);
        s->cpuDevice = INVALID_HANDLE_VALUE;
    }
    s->cpuBaseline = FALSE;
    if (s->started)
    {
        counter("PerfStats\\StopStat", &value, &type, &size);
        s->started = FALSE;
    }
}
/* Caller provides at least 512 bytes. Fixed strings and DWORDs bound the output. */
void telemetry_text(TELEMETRY *s, char *text)
{
    telemetry_sample(s);
    sprintf(text, "%ld|%lu|%lu|%lu|%lu|%lu|%lu|%lu|%s|%s|%lu|%lu|%lu|%lu|%lu|%lu",
            s->cpu, s->memory.dwMemoryLoad, s->memory.dwTotalPhys / 1024, s->memory.dwAvailPhys / 1024,
            s->memory.dwTotalPageFile / 1024, s->memory.dwAvailPageFile / 1024,
            s->memory.dwTotalVirtual / 1024, s->memory.dwAvailVirtual / 1024,
            s->brand, s->vendor, s->family, s->model, s->stepping, s->processors, s->frequencyMHz, s->frequencyKind);
}
