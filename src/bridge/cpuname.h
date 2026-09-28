/* Conservative names for processors predating CPUID brand strings.
   Intel AP-485 and AMD Processor Recognition / K6 BIOS Design Guide.
   Shared signatures retain alternatives instead of inventing a precise SKU. */
#ifndef W98_CPUNAME_H
#define W98_CPUNAME_H
#include <string.h>
static const char *legacy_cpu_name(const char *vendor, unsigned long family, unsigned long model)
{
    if (!strcmp(vendor, "GenuineIntel"))
    {
        if (family == 4)
        {
            switch (model)
            {
            case 0:
            case 1:
                return "Intel i486 DX";
            case 2:
                return "Intel i486 SX";
            case 3:
            case 7:
                return "Intel i486 DX2";
            case 4:
                return "Intel i486 SL";
            case 5:
                return "Intel i486 SX2";
            case 8:
            case 9:
                return "Intel i486 DX4";
            }
            return "Intel i486";
        }
        if (family == 5)
        {
            if (model == 4 || model == 8)
                return "Intel Pentium MMX";
            if (model <= 3 || model == 7)
                return "Intel Pentium";
        }
        if (family == 6)
        {
            switch (model)
            {
            case 1:
                return "Intel Pentium Pro";
            case 3:
                return "Intel Pentium II";
            case 5:
                return "Intel Pentium II / Celeron / Xeon";
            case 6:
                return "Intel Celeron / Mobile Pentium II";
            case 7:
                return "Intel Pentium III / Pentium III Xeon";
            case 8:
                return "Intel Pentium III / Celeron / Xeon";
            case 11:
                return "Intel Pentium III / Celeron";
            case 10:
                return "Intel Pentium III Xeon";
            case 9:
            case 13:
                return "Intel Pentium M / Celeron M";
            }
        }
    }
    if (!strcmp(vendor, "AuthenticAMD"))
    {
        if (family == 4)
        {
            if (model == 14 || model == 15)
                return "AMD Am5x86";
            return "AMD Am486";
        }
        if (family == 5)
        {
            if (model <= 3)
                return "AMD K5";
            if (model == 6 || model == 7)
                return "AMD K6";
            if (model == 8)
                return "AMD K6-2";
            if (model == 9)
                return "AMD K6-III";
            if (model == 13)
                return "AMD K6-2+ / K6-III+";
        }
        if (family == 6)
        {
            if (model == 1 || model == 2 || model == 4)
                return "AMD Athlon";
            if (model == 3 || model == 7)
                return "AMD Duron";
        }
    }
    return NULL;
}
static const char *intel_legacy_detail(unsigned long signature, unsigned long brand, int l2)
{
    unsigned long family = (signature >> 8) & 15, model = (signature >> 4) & 15;
    if (((signature >> 12) & 3) == 1 && !(signature & 0x0FFF0000UL))
    {
        if (family == 6 && model == 3)
            return "Intel Pentium II OverDrive";
        if (family == 5 && model == 4)
            return "Intel Pentium OverDrive MMX";
        if (family == 5 && model >= 1 && model <= 3)
            return "Intel Pentium OverDrive";
        if (family == 4 && model == 8)
            return "Intel i486 DX4 OverDrive";
        return "Intel OverDrive";
    }
    if (family != 6 || (signature & 0x0FFF0000UL))
        return NULL;
    if (model == 5)
    {
        if (l2 == 0)
            return "Intel Celeron";
        if (l2 >= 1024)
            return "Intel Pentium II Xeon";
        if (l2 == 512)
            return "Intel Pentium II / Pentium II Xeon";
    }
    if (model == 6)
    {
        if (l2 == 128)
            return "Intel Celeron";
        if (l2 == 256)
            return "Intel Mobile Pentium II";
    }
    if (model == 7 && l2 >= 1024)
        return "Intel Pentium III Xeon";
    if (model == 8 || model == 10 || model == 11)
    {
        switch (brand)
        {
        case 1:
            return "Intel Celeron";
        case 2:
        case 4:
            return "Intel Pentium III";
        case 3:
            return signature == 0x6B1 ? "Intel Celeron" : "Intel Pentium III Xeon";
        case 6:
            return "Intel Mobile Pentium III-M";
        case 7:
            return "Intel Mobile Celeron";
        }
    }
    return NULL;
}
static const char *amd_legacy_detail(unsigned long family, unsigned long model, unsigned long l2)
{
    if (family == 5 && model == 13)
    {
        if (l2 == 128)
            return "AMD K6-2+";
        if (l2 == 256)
            return "AMD K6-III+";
    }
    return NULL;
}
#endif
