/* Offline lookup and CD time-format tests; no physical drive is accessed. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int restored, badTrack, layoutQueries;
static MCIERROR WINAPI fake_mci(LPCSTR command, LPSTR text, UINT size, HWND window)
{
    (void)window;
    if (!strcmp(command, "set w98cd time format msf"))
    {
        restored = 0;
        return 0;
    }
    if (!strcmp(command, "set w98cd time format tmsf"))
    {
        restored = 1;
        return 0;
    }
    if (!strcmp(command, "info w98cd identity"))
        lstrcpynA(text, "MOCKDISC", size);
    else if (strstr(command, "type track"))
        lstrcpynA(text, badTrack ? "other" : "audio", size);
    else if (strstr(command, "position track 1"))
    {
        layoutQueries++;
        lstrcpynA(text, "00:02:00", size);
    }
    else if (strstr(command, "position track 2"))
        lstrcpynA(text, "03:22:00", size);
    else if (strstr(command, "length track 2"))
        lstrcpynA(text, "03:20:00", size);
    else
        return 1;
    return 0;
}
#define mciSendStringA fake_mci
static MCIDEVICEID WINAPI fake_device(LPCSTR name)
{
    assert(!strcmp(name, "w98cd"));
    return 1;
}
static MCIERROR WINAPI fake_command(MCIDEVICEID device, UINT message, DWORD_PTR flags, DWORD_PTR parameters)
{
    MCI_STATUS_PARMS *status = (MCI_STATUS_PARMS*)parameters;
    assert(device == 1 && message == MCI_STATUS && flags == (MCI_STATUS_ITEM | MCI_TRACK));
    assert(status->dwItem == MCI_CDA_STATUS_TYPE_TRACK);
    status->dwReturn = badTrack ? MCI_CDA_TRACK_OTHER : MCI_CDA_TRACK_AUDIO;
    return 0;
}
#define mciGetDeviceIDA fake_device
#define mciSendCommandA fake_command
#include "../src/bridge/details.c"
void js_string(FILE *f, const char *s)
{
    fprintf(f, "'%s'", s);
}
int main(int argc, char **argv)
{
    unsigned long offsets[99], duration;
    static CD_CATALOG_RESULT result;
    int matches, tracks = 2, i, choice = argc > 2 ? atoi(argv[2]) : 0;
    DWORD started;
    char directory[MAX_PATH], iniPath[MAX_PATH], output[4096], *slash;
    FILE *snapshot;
    int queries, bytes;
    assert(cd_layout(2, offsets, &duration));
    assert(restored && offsets[0] == 0 && offsets[1] == 15000 && duration == 30000);
    badTrack = 1;
    assert(!cd_layout(2, offsets, &duration));
    assert(restored);
    badTrack = 0;
    assert(cd_layout(2, offsets, &duration));
    if (argc < 2)
        return 2;
    if (argc > 3)
        duration = strtoul(argv[3], NULL, 10);
    if (argc > 5 && !strcmp(argv[4], "catalog"))
    {
        snapshot = fopen(argv[5], "r");
        assert(snapshot && fscanf(snapshot, "%d %lu", &tracks, &duration) == 2 && tracks > 0 && tracks <= 99);
        for (i = 0; i < tracks; i++)
            assert(fscanf(snapshot, "%lu", &offsets[i]) == 1);
        fclose(snapshot);
    }
    started = GetTickCount();
    matches = cd_catalog_lookup(argv[1], offsets, tracks, duration, choice, &result);
    if (argc > 5)
        fprintf(stderr, "Lookup: %lu ms\n", GetTickCount() - started);
    printf("%d\n%s\n%s\n%s\n%s\n", matches, result.album, result.artist, result.tracks[0], result.tracks[1]);
    if (argc > 4 && !strcmp(argv[4], "integration"))
    {
        lstrcpynA(directory, argv[1], sizeof(directory));
        slash = strrchr(directory, '\\');
        assert(slash);
        *slash = 0;
        sprintf(iniPath, "%s\\CDTITLES.INI", directory);
        snapshot = tmpfile();
        assert(snapshot);
        details_cd(snapshot, directory, 1, 1, 2);
        queries = layoutQueries;
        assert(queries > 0);
        rewind(snapshot);
        bytes = fread(output, 1, sizeof(output) - 1, snapshot);
        output[bytes] = 0;
        assert(strstr(output, result.album));
        fclose(snapshot);
        assert(WritePrivateProfileStringA("CD_MOCKDISC_2", "Album", "My local title", iniPath));
        snapshot = tmpfile();
        assert(snapshot);
        details_cd(snapshot, directory, 1, 2, 2);
        assert(layoutQueries == queries);
        rewind(snapshot);
        bytes = fread(output, 1, sizeof(output) - 1, snapshot);
        output[bytes] = 0;
        assert(strstr(output, "My local title"));
        fclose(snapshot);
        DeleteFileA(iniPath);
        sprintf(iniPath, "%s\\CDLOOKUP.TMP", directory);
        DeleteFileA(iniPath);
    }
    return 0;
}
