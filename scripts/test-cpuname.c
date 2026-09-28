#include <stdio.h>
#include "../src/bridge/cpuname.h"
#include "../src/bridge/cpufreq.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    CHECK(!strcmp(legacy_cpu_name("GenuineIntel", 6, 3), "Intel Pentium II"));
    CHECK(!strcmp(legacy_cpu_name("GenuineIntel", 6, 1), "Intel Pentium Pro"));
    CHECK(!strcmp(legacy_cpu_name("GenuineIntel", 5, 4), "Intel Pentium MMX"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 5, 8), "AMD K6-2"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 5, 9), "AMD K6-III"));
    CHECK(!legacy_cpu_name("GenuineIntel", 6, 99));
    CHECK(!legacy_cpu_name("UnknownBrand", 6, 3));
    CHECK(!strcmp(intel_legacy_detail(0x650, 0, 0), "Intel Celeron"));
    CHECK(!strcmp(intel_legacy_detail(0x650, 0, 1024), "Intel Pentium II Xeon"));
    CHECK(strstr(intel_legacy_detail(0x650, 0, 512), " / ") != NULL);
    CHECK(!intel_legacy_detail(0x650, 0, -1));
    CHECK(!strcmp(intel_legacy_detail(0x660, 0, 128), "Intel Celeron"));
    CHECK(!strcmp(intel_legacy_detail(0x660, 0, 256), "Intel Mobile Pentium II"));
    CHECK(!strcmp(intel_legacy_detail(0x680, 2, -1), "Intel Pentium III"));
    CHECK(!strcmp(intel_legacy_detail(0x6B1, 3, -1), "Intel Celeron"));
    CHECK(!intel_legacy_detail(0x106A0, 3, 256));
    CHECK(!strcmp(intel_legacy_detail(0x1632, 0, 512), "Intel Pentium II OverDrive"));
    CHECK(!intel_legacy_detail(0x632, 0, 512));
    CHECK(!intel_legacy_detail(0x2632, 0, 512));
    CHECK(!intel_legacy_detail(0x3632, 0, 512));
    CHECK(!strcmp(intel_legacy_detail(0x1543, 0, 0), "Intel Pentium OverDrive MMX"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 5, 0), "AMD K5"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 5, 7), "AMD K6"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 6, 4), "AMD Athlon"));
    CHECK(!strcmp(legacy_cpu_name("AuthenticAMD", 6, 7), "AMD Duron"));
    CHECK(!legacy_cpu_name("AuthenticAMD", 5, 12));
    CHECK(!legacy_cpu_name("AuthenticAMD", 25, 1));
    CHECK(!strcmp(amd_legacy_detail(5, 13, 128), "AMD K6-2+"));
    CHECK(!strcmp(amd_legacy_detail(5, 13, 256), "AMD K6-III+"));
    CHECK(!amd_legacy_detail(5, 13, 0));
    CHECK(!amd_legacy_detail(6, 13, 256));
    CHECK(frequency_result(299.7, 300.2, 299.9) == 300);
    CHECK(frequency_result(300, 600, 300) == 0);
    CHECK(frequency_result(0, 0, 0) == 0);
    CHECK(frequency_result(-300, 300, 300) == 0);
    CHECK(frequency_result(30000, 30000, 30000) == 0);
    puts("PASS: legacy CPU names, cache/brand disambiguation, unknown signatures");
    return 0;
}
