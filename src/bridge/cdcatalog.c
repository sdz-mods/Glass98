/* Bounded, indexed reader for the optional MusicBrainz CC0 CD catalog. */
#include <stdio.h>
#include <string.h>
#include "cdcatalog.h"

static unsigned long number(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static unsigned long signature(const unsigned long *offsets, int tracks)
{
    unsigned long hash = 2166136261UL, value;
    int i, j;
    for (i = -1; i < tracks; i++)
    {
        value = i < 0 ? (unsigned long)tracks : offsets[i];
        for (j = 0; j < 4; j++)
        {
            hash = ((hash ^ (value & 255)) * 16777619UL) & 0xffffffffUL;
            value >>= 8;
        }
    }
    return hash;
}

static int entry(FILE *file, unsigned long start, unsigned long index, unsigned long *out)
{
    unsigned char bytes[16];
    int i;
    if (fseek(file, (long)(start + index * 16), SEEK_SET) || fread(bytes, 1, 16, file) != 16)
        return 0;
    for (i = 0; i < 4; i++)
        out[i] = number(bytes + i * 4);
    return 1;
}

int cd_catalog_lookup(const char *path, const unsigned long *offsets, int tracks,
                      unsigned long duration, int choice, CD_CATALOG_RESULT *result)
{
    FILE *file;
    unsigned char header[32], payload[53248], *p, *end, *zero;
    unsigned long count, index, low, high, mid, row[4], hash, floor, length;
    long size;
    int i, found = 0, scanned = 0, valid;
    char *target;
    memset(result, 0, sizeof(*result));
    if (tracks < 1 || tracks > 99 || offsets[0] || duration < 1 || duration > 1000000)
        return -1;
    for (i = 1; i < tracks; i++)
        if (offsets[i] <= offsets[i - 1] || offsets[i] >= duration)
            return -1;
    file = fopen(path, "rb");
    if (!file)
        return 0;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 32 || fseek(file, 0, SEEK_SET) ||
            fread(header, 1, 32, file) != 32 || memcmp(header, "G98CDDB1", 8))
        goto invalid;
    count = number(header + 8);
    index = number(header + 12);
    if (index < 32 || index > (unsigned long)size || count > ((unsigned long)size - index) / 16 ||
            index + count * 16 != (unsigned long)size)
        goto invalid;
    hash = signature(offsets, tracks);
    floor = duration > 2 ? duration - 2 : 0;
    low = 0;
    high = count;
    while (low < high)
    {
        mid = low + (high - low) / 2;
        if (!entry(file, index, mid, row))
            goto invalid;
        if (row[0] < hash || (row[0] == hash && row[1] < floor))
            low = mid + 1;
        else
            high = mid;
    }
    for (; low < count; low++)
    {
        if (!entry(file, index, low, row))
            goto invalid;
        if (row[0] != hash || row[1] > duration + 2)
            break;
        if (++scanned > 256 || row[2] < 32 || row[2] > index || row[3] > index - row[2] ||
                row[3] > sizeof(payload) || row[3] < 4 + (unsigned long)tracks * 4)
            goto invalid;
        if (fseek(file, (long)row[2], SEEK_SET) || fread(payload, 1, row[3], file) != row[3])
            goto invalid;
        if (number(payload) != (unsigned long)tracks)
            continue;
        valid = 1;
        for (i = 0; i < tracks; i++)
            if (number(payload + 4 + i * 4) != offsets[i])
                valid = 0;
        if (!valid)
            continue;
        p = payload + 4 + tracks * 4;
        end = payload + row[3];
        for (i = -2; i < tracks; i++)
        {
            zero = memchr(p, 0, (size_t)(end - p));
            if (!zero || zero - p >= CD_CATALOG_TEXT)
                goto invalid;
            length = (unsigned long)(zero - p);
            if (!found || found == choice)
            {
                target = i == -2 ? result->album : i == -1 ? result->artist : result->tracks[i];
                memcpy(target, p, length);
                target[length] = 0;
            }
            p = zero + 1;
        }
        if (p != end)
            goto invalid;
        found++;
    }
    fclose(file);
    return found;
invalid:
    fclose(file);
    memset(result, 0, sizeof(*result));
    return -1;
}
