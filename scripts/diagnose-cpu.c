/* Read-only hardware diagnostic for the optional Glass98 CPU driver. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../src/vxd/g98cpu.h"
#include "../src/bridge/cpumeter.h"

static void file_info(FILE *log, const char *path)
{
    FILE *file;
    BYTE bytes[1024];
    DWORD crc = 0xffffffffUL, total = 0, attributes, error;
    size_t count, i;
    int bit;
    file = fopen(path, "rb");
    if (!file)
    {
        attributes = GetFileAttributesA(path);
        error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
        fprintf(log, "File %s: unavailable (attributes=%08lx error=%lu)\n",
                path, attributes, error);
        return;
    }
    while ((count = fread(bytes, 1, sizeof(bytes), file)) != 0)
    {
        total += count;
        for (i = 0; i < count; i++)
        {
            crc ^= bytes[i];
            for (bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
        }
    }
    fprintf(log, "File %s: bytes=%lu crc32=%08lx readError=%d\n", path, total, ~crc, ferror(file));
    fclose(file);
}

static void registration(FILE *log)
{
    HKEY key;
    char value[MAX_PATH + 1];
    BYTE start[16];
    DWORD size, type;
    LONG error;
    error = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                         "System\\CurrentControlSet\\Services\\VxD\\G98CPU", 0, KEY_READ, &key);
    fprintf(log, "Driver registration open error=%ld\n", error);
    if (error)
        return;
    size = MAX_PATH;
    type = 0;
    memset(value, 0, sizeof(value));
    error = RegQueryValueExA(key, "StaticVxD", NULL, &type, (BYTE *)value, &size);
    fprintf(log, "StaticVxD error=%ld type=%lu bytes=%lu path=%s\n", error, type, size, error ? "?" : value);
    if (!error && type == REG_SZ && size <= MAX_PATH)
        file_info(log, value);
    size = sizeof(start);
    type = 0;
    memset(start, 0, sizeof(start));
    error = RegQueryValueExA(key, "Start", NULL, &type, start, &size);
    fprintf(log, "Start error=%ld type=%lu bytes=%lu firstByte=%u\n", error, type, size, start[0]);
    RegCloseKey(key);
}

static void text_file(FILE *log, const char *path)
{
    FILE *file = fopen(path, "rb");
    char buffer[512];
    size_t size;
    if (!file)
        return;
    size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[size] = 0;
    fprintf(log, "%s: %s\n", path, buffer);
    fclose(file);
}

int main(void)
{
    char path[MAX_PATH], method[32], *name;
    DWORD length, error, returned, tick, previousTick = 0;
    OSVERSIONINFOA os;
    SYSTEM_INFO system;
    G98CPU_SAMPLE sample, previous;
    HANDLE device;
    FILE *log;
    int i, baseline = 0;
    double elapsed, idle;

    length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (!length || length >= sizeof(path) || !(name = strrchr(path, '\\')) ||
        (size_t)(name + 1 - path) + sizeof("CPUDIAG.TXT") > sizeof(path))
        return 1;
    strcpy(name + 1, "CPUDIAG.TXT");
    log = fopen(path, "w");
    if (!log)
    {
        puts("Cannot create CPUDIAG.TXT beside this program. Use a writable folder.");
        return 1;
    }
    printf("Glass98 CPU diagnostic. Please wait about 12 seconds.\nLog: %s\n", path);
    fprintf(log, "Glass98 CPU diagnostic 1\nNo timer requests, registry writes or deliberate CPU load.\n");
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    GetSystemInfo(&system);
    fprintf(log, "Windows=%lu.%lu build=%lu platform=%lu %s\nCPU type=%lu level=%u revision=%u count=%lu\n",
            os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber & 65535, os.dwPlatformId,
            os.szCSDVersion, system.dwProcessorType, system.wProcessorLevel, system.wProcessorRevision,
            system.dwNumberOfProcessors);
    GetPrivateProfileStringA("CPU", "Method", "(missing; defaults to Legacy)", method, sizeof(method), "C:\\Glass98\\WIDGETS.INI");
    fprintf(log, "Installed CPU method=%s\n", method);
    registration(log);
    file_info(log, "C:\\Glass98\\G98CPU.VXD");
    file_info(log, "C:\\Glass98\\W98DATA.EXE");
    text_file(log, "C:\\Glass98\\VERSION.TXT");
    text_file(log, "C:\\Glass98\\RUNSTATE.JS");
    text_file(log, "C:\\Glass98\\DATA.JS");
    if (os.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS)
    {
        fprintf(log, "Driver probe skipped: not Windows 9x.\n");
        fclose(log);
        return 2;
    }
    SetLastError(0);
    device = CreateFileA("\\\\.\\G98CPU", 0, 0, NULL, 0, 0x10000000UL, NULL);
    error = device == INVALID_HANDLE_VALUE ? GetLastError() : 0;
    fprintf(log, "Open resident G98CPU: error=%lu\n", error);
    if (device == INVALID_HANDLE_VALUE)
    {
        printf("Driver open failed (error %lu). Details saved in CPUDIAG.TXT.\n", error);
        fclose(log);
        return 3;
    }
    for (i = 0; i < 12; i++)
    {
        memset(&sample, 0, sizeof(sample));
        returned = 0;
        tick = GetTickCount();
        if (!DeviceIoControl(device, G98CPU_READ, NULL, 0, &sample, sizeof(sample), &returned, NULL))
        {
            fprintf(log, "Sample %d: IOCTL error=%lu bytes=%lu\n", i, GetLastError(), returned);
            baseline = 0;
        }
        else
        {
            fprintf(log, "Sample %d: tick=%lu bytes=%lu size=%lu version=%lu system=%lu idle=%lu caller=%lu pit=%lu:%lu\n",
                    i, tick, returned, sample.size, sample.version, sample.systemMs,
                    sample.systemThreadMs, sample.callerThreadMs, sample.pitHigh, sample.pitLow);
            if (baseline)
            {
                elapsed = (((double)sample.pitHigh - previous.pitHigh) * 4294967296.0 +
                           (double)sample.pitLow - previous.pitLow) / 1193.182;
                idle = (DWORD)(sample.systemThreadMs - previous.systemThreadMs);
                fprintf(log, "  widgetCPU=%ld wallMs=%lu elapsedMs=%.3f idleMs=%.3f rawCPU=%.3f\n",
                        cpu_meter(&previous, &sample), (DWORD)(tick - previousTick), elapsed, idle,
                        elapsed > 0.0 ? 100.0 * (1.0 - idle / elapsed) : -999.0);
            }
            previous = sample;
            previousTick = tick;
            baseline = returned == sizeof(sample) && sample.size == sizeof(sample) && sample.version == G98CPU_VERSION;
        }
        fflush(log);
        Sleep(1000);
    }
    CloseHandle(device);
    text_file(log, "C:\\Glass98\\DATA.JS");
    fclose(log);
    puts("Done. Please return CPUDIAG.TXT for analysis.");
    return 0;
}
