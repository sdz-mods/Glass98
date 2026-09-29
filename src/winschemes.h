/* Use the target Windows installation's Appearance schemes and Display applet. */
#ifndef GLASS98_WINSCHEMES_H
#define GLASS98_WINSCHEMES_H
#define SCHEME_KEY "Control Panel\\Appearance\\Schemes"
#define SCHEME_NAME 256
static HMODULE windows_scheme_library(void)
{
    char path[MAX_PATH];
    UINT length = GetSystemDirectoryA(path, sizeof(path));
    if (!length || length > sizeof(path) - 10)
        return NULL;
    strcat(path, "\\DESK.CPL");
    return LoadLibraryA(path);
}
static int windows_scheme_decode(const char *hex, char *name)
{
    unsigned int value;
    size_t i, length = strlen(hex);
    if (!length || length % 2 || length / 2 >= SCHEME_NAME)
        return 0;
    for (i = 0; i < length; i += 2)
    {
        if (!isxdigit((unsigned char)hex[i]) || !isxdigit((unsigned char)hex[i + 1]) ||
                sscanf(hex + i, "%2x", &value) != 1 || value < 32)
            return 0;
        name[i / 2] = (char)value;
    }
    name[length / 2] = 0;
    return 1;
}
static int windows_scheme_apply(const char *hex)
{
    typedef BOOL (WINAPI *APPLY_SCHEME)(LPCSTR);
    HMODULE library;
    HKEY key;
    DWORD type, size = 0;
    char name[SCHEME_NAME];
    APPLY_SCHEME apply;
    int ok = 0;
    if (!windows_scheme_decode(hex, name) || RegOpenKeyA(HKEY_CURRENT_USER, SCHEME_KEY, &key))
        return 0;
    if (RegQueryValueExA(key, name, NULL, &type, NULL, &size) || type != REG_BINARY || !size)
    {
        RegCloseKey(key);
        return 0;
    }
    RegCloseKey(key);
    library = windows_scheme_library();
    if (!library)
        return 0;
    apply = (APPLY_SCHEME)GetProcAddress(library, "DeskSetCurrentSchemeA");
    if (apply)
        ok = apply(name) != FALSE;
    FreeLibrary(library);
    return ok;
}
#endif
