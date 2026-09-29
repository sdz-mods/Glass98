/* Native profile tests: run in build/ on the host and on Win98. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/glassctl/themes.h"
#define CHECK(x) do{if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);failed=1;goto done;}}while(0)
int main(void)
{
    char path[MAX_PATH], full[MAX_PATH], layout[4096], original[4096], graphite[4096], row[128], v[4096], cmd[128],
         name[40], output[8192];
    int i, j, failed = 0;
    char *cursor;
    FILE *f;
    if (!GetTempFileNameA(".", "wth", 0, path) || !GetFullPathNameA(path, sizeof(full), full, NULL))
        return 2;
    strcpy(layout, "3|333|1|0|3");
    for (i = 0; i < 28; i++)
    {
        sprintf(row, "|%d|%d|%d|%06X|F0F0F0|AABBCC|%d|%d", i % 2, i * 7, i * 9, i * 101, 20 + i, 2 + i);
        strcat(layout, row);
    }
    strcpy(original, layout);
    WritePrivateProfileStringA("Desktop", "Layout", layout, full);
    WritePrivateProfileStringA("Desktop", "Wallpaper", "C:\\KEEP.BMP", full);
    WritePrivateProfileStringA("Options", "notes", "Keep these notes", full);
    CHECK(theme_action("new/4f726967696e616c", full, layout));
    CHECK(!strcmp(layout, original));
    CHECK(theme_action("apply/1", full, layout));
    strcpy(graphite, layout);
    CHECK(strcmp(layout, original));
    CHECK(!strncmp(layout, "3|333|1|0|3|", 12));
    CHECK(GetPrivateProfileIntA("Options", "meterColors", 0, full) == 1);
    GetPrivateProfileStringA("Options", "notes", "", v, sizeof(v), full);
    CHECK(!strcmp(v, "Keep these notes"));
    GetPrivateProfileStringA("Desktop", "Wallpaper", "", v, sizeof(v), full);
    CHECK(!strcmp(v, "C:\\KEEP.BMP"));
    CHECK(theme_action("apply/100", full, layout));
    CHECK(!strcmp(layout, original));
    CHECK(GetPrivateProfileIntA("Options", "meterColors", 1, full) == 0);
    CHECK(theme_action("apply/1000", full, layout));
    CHECK(!strcmp(layout, graphite));
    CHECK(theme_action("apply/1000", full, layout));
    CHECK(!strcmp(layout, original));
    CHECK(!theme_action("delete/0", full, layout));
    CHECK(!theme_action("update/1", full, layout));
    CHECK(!theme_action("apply/999", full, layout));
    CHECK(!theme_action("meter/1,BAD,112233,445566", full, layout));
    CHECK(!theme_action("new/00", full, layout));
    CHECK(!theme_action("new/2222", full, layout));
    CHECK(!theme_action("new/4f726967696e616c", full, layout));
    CHECK(theme_action("meter/1,112233,445566,778899", full, layout));
    CHECK(theme_action("new/3c277468656d653e", full, layout));
    CHECK(theme_action("apply/4", full, layout));
    CHECK(theme_action("update/101", full, layout));
    CHECK(theme_action("apply/0", full, layout));
    CHECK(theme_action("apply/101", full, layout));
    CHECK(strstr(layout, "F4F1E8|202B35|245F95|96") != NULL);
    f = tmpfile();
    CHECK(f != NULL);
    theme_publish(f, full, layout);
    rewind(f);
    i = fread(output, 1, sizeof(output) - 1, f);
    output[i] = 0;
    fclose(f);
    CHECK(strstr(output, "themeActive=101") != NULL);
    CHECK(strstr(output, "\\x3c\\x27theme>") != NULL);
    CHECK(theme_action("delete/101", full, layout));
    CHECK(!theme_action("apply/101", full, layout));
    for (i = 1; i < 16; i++)
    {
        sprintf(name, "Custom%d", i);
        strcpy(cmd, "new/");
        for (j = 0; name[j]; j++)
            sprintf(cmd + 4 + j * 2, "%02x", (unsigned char)name[j]);
        CHECK(theme_action(cmd, full, layout));
    }
    CHECK(!theme_action("new/46756c6c", full, layout));
    CHECK(theme_action("apply/100", full, layout));
    CHECK(!strcmp(layout, original));
    puts("PASS: preset apply, full appearance restore, previous snapshot swap, preserved content/layout, custom replace/delete, validation, escaping and 16-theme limit");
    GetPrivateProfileStringA("Theme0", "Data", "", v, sizeof(v), full);
    cursor = v;
    for (i = 0; i < 93; i++)
    {
        cursor = strchr(cursor, '|');
        CHECK(cursor != NULL);
        if (i < 92)
            cursor++;
    }
    *cursor = 0;
    CHECK(WritePrivateProfileStringA("Theme0", "Data", v, full));
    CHECK(theme_action("apply/100", full, layout));
    strcpy(original, "3|333|1|0|3");
    for (i = 0; i < 28; i++)
    {
        sprintf(row, "|%d|%d|%d|%06X|F0F0F0|AABBCC|%d|%d", i % 2, i * 7, i * 9,
                i < 22 ? i * 101 : 0, i < 22 ? 20 + i : 20, 2 + i);
        strcat(original, row);
    }
    CHECK(!strcmp(layout, original));
    memset(v, 'A', 150);
    strcpy(v + 150, "|FFFFFF|FFFFFF|55");
    strcpy(output, "1|0|112233|445566|778899|");
    strcat(output, v);
    for (i = 1; i < 22; i++)
        strcat(output, "|000000|FFFFFF|FFFFFF|55");
    CHECK(WritePrivateProfileStringA("Theme0", "Data", output, full));
    CHECK(!theme_action("apply/100", full, layout));
    puts("PASS: A00 saved themes extend to new widgets and malformed colors are rejected");
done:
    WritePrivateProfileStringA(NULL, NULL, NULL, full);
    DeleteFileA(full);
    return failed;
}
