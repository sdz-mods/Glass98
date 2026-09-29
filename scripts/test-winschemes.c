#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "../src/winschemes.h"
static void encode(const char *name, char *hex)
{
    while (*name)
    {
        sprintf(hex, "%02X", (unsigned char)*name++);
        hex += 2;
    }
    *hex = 0;
}
int main(int argc, char **argv)
{
    char name[SCHEME_NAME], previous[SCHEME_NAME], hex[SCHEME_NAME * 2];
    HKEY key;
    DWORD size, type;
    COLORREF before, after;
    int applied, restored;
    assert(windows_scheme_decode("536C617465", name) && !strcmp(name, "Slate"));
    assert(!windows_scheme_decode("00", name));
    assert(!windows_scheme_decode("123", name));
    assert(!windows_scheme_decode("ZZ", name));
    assert(!windows_scheme_apply("00"));
    puts("PASS: scheme command validation");
    if (argc < 2 || strcmp(argv[1], "--exercise"))
        return 0;
    assert(GetVersion() & 0x80000000UL);
    assert(!RegOpenKeyA(HKEY_CURRENT_USER, "Control Panel\\Appearance", &key));
    size = sizeof(previous);
    assert(!RegQueryValueExA(key, "Current", NULL, &type, (BYTE*)previous, &size) && type == REG_SZ);
    RegCloseKey(key);
    previous[sizeof(previous) - 1] = 0;
    before = GetSysColor(COLOR_BTNFACE);
    encode(!strcmp(previous, "Slate") ? "Wheat" : "Slate", hex);
    applied = windows_scheme_apply(hex);
    after = GetSysColor(COLOR_BTNFACE);
    encode(previous, hex);
    restored = windows_scheme_apply(hex);
    printf("Original=%s applied=%d before=%08lX after=%08lX restored=%d final=%08lX\n",
           previous, applied, before, after, restored, GetSysColor(COLOR_BTNFACE));
    assert(applied && restored && before != after && GetSysColor(COLOR_BTNFACE) == before);
    puts("PASS: native Windows Appearance scheme applied and original scheme restored");
    return 0;
}
