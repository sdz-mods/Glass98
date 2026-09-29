/* Exercise optional setup without touching the real C:\Glass98 installation. */
#include <windows.h>
#include <assert.h>
static int answer = IDYES;
static int WINAPI question(HWND w, LPCSTR text, LPCSTR title, UINT flags)
{
    (void)w;
    (void)title;
    assert(strstr(text, "Disk space:"));
    assert(flags & MB_DEFBUTTON2);
    return answer;
}
#define MessageBoxA question
#define TARGET "catalog-install-test"
#define main setup_main
#include "../src/glassctl/setup.c"
#undef main
int main(int argc, char **argv)
{
    HANDLE held;
    FILE *f;
    char magic[8];
    assert(argc == 2);
    CreateDirectoryA(TARGET, NULL);
    assert(choose_catalog(argv[1]) == 1);
    answer = IDNO;
    assert(choose_catalog(argv[1]) == 0);
    answer = IDCANCEL;
    assert(choose_catalog(argv[1]) == -1);
    assert(choose_catalog("missing-catalog-source") == 0);
    assert(install_catalog(argv[1]));
    held = CreateFileA(TARGET "\\CDMETA.DAT", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(held != INVALID_HANDLE_VALUE);
    assert(!install_catalog(argv[1]));
    CloseHandle(held);
    assert(install_catalog(argv[1]));
    f = fopen(TARGET "\\CDMETA.DAT", "rb");
    assert(f && fread(magic, 1, 8, f) == 8 && !memcmp(magic, "G98CDDB1", 8));
    fclose(f);
    assert(!exists(TARGET "\\CDMETA.BAK"));
    DeleteFileA(TARGET "\\CDMETA.DAT");
    DeleteFileA(TARGET "\\CDDATA.TXT");
    DeleteFileA(TARGET "\\CC0.TXT");
    RemoveDirectoryA(TARGET);
    puts("PASS: optional install Yes/No/Cancel, missing catalog, locked destination and replacement");
    return 0;
}
