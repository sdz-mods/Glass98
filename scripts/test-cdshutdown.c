/* Exercise CD ownership and stop-before-close behavior without an audio driver. */
#include <windows.h>
#include <mmsystem.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char trace[2048];
static DWORD owner;
static int currentTrack = 1, totalTracks = 8;
static MCIERROR statusError;
static HANDLE openedEvent, playedEvent;
static MCIERROR WINAPI test_mci(LPCSTR command, LPSTR result, UINT size, HWND callback)
{
    (void)callback;
    if (!owner)
        owner = GetCurrentThreadId();
    assert(owner == GetCurrentThreadId());
    assert(strlen(trace) + strlen(command) + 2 < sizeof(trace));
    strcat(trace, command);
    strcat(trace, "\n");
    if (result && size)
    {
        lstrcpynA(result, "1", size);
        if (!strcmp(command, "status w98cd number of tracks"))
        {
            if (statusError)
                return statusError;
            sprintf(result, "%d", totalTracks);
        }
        if (!strcmp(command, "status w98cd current track"))
            sprintf(result, "%d", currentTrack);
    }
    if (openedEvent && !strncmp(command, "open ", 5))
        SetEvent(openedEvent);
    if (playedEvent && !strncmp(command, "play ", 5))
        SetEvent(playedEvent);
    return 0;
}
static UINT WINAPI test_drive(LPCSTR path)
{
    (void)path;
    return DRIVE_CDROM;
}
#define mciSendStringA test_mci
#define GetDriveTypeA test_drive
#include "../src/bridge/extras.c"
#ifdef GLASS98_PANELS
/* The shutdown test isolates the media worker from unrelated telemetry. */
void details_collect(FILE *f, const char *p, int *e, int s) { (void)f; (void)p; (void)e; (void)s; }
#endif
void details_cd(FILE *f, const char *p, int o, int t, int n) { (void)f; (void)p; (void)o; (void)t; (void)n; }
int main(void)
{
    char directory[MAX_PATH], config[4096] = "3|280|1|0|0", row[100], path[MAX_PATH];
    int i;
    assert(perform_action("cd/open/D"));
    assert(perform_action("cd/play"));
    currentTrack = 8;
    trace[0] = 0;
    assert(perform_action("cd/next"));
    assert(strstr(trace, "play w98cd from 1\n"));
    assert(!actionStatus[0]);
    currentTrack = 1;
    trace[0] = 0;
    assert(perform_action("cd/prev"));
    assert(strstr(trace, "play w98cd from 8\n"));
    currentTrack = 4;
    trace[0] = 0;
    assert(perform_action("cd/next"));
    assert(strstr(trace, "play w98cd from 5\n"));
    trace[0] = 0;
    assert(perform_action("cd/prev"));
    assert(strstr(trace, "play w98cd from 3\n"));
    currentTrack = totalTracks = 1;
    trace[0] = 0;
    assert(perform_action("cd/next"));
    assert(perform_action("cd/prev"));
    assert(!strstr(trace, "play w98cd from 0\n"));
    assert(!strstr(trace, "play w98cd from 2\n"));
    totalTracks = 0;
    trace[0] = 0;
    assert(!perform_action("cd/next"));
    assert(!strstr(trace, "play w98cd"));
    assert(actionStatus[0]);
    totalTracks = 8;
    statusError = MCIERR_OUTOFRANGE;
    trace[0] = 0;
    assert(!perform_action("cd/prev"));
    assert(!strstr(trace, "play w98cd"));
    assert(actionStatus[0]);
    statusError = 0;
    assert(perform_action("cd/next"));
    assert(!actionStatus[0]);
    trace[0] = 0;
    close_cd();
    assert(!strcmp(trace, "stop w98cd wait\nclose w98cd wait\n"));
    close_cd();
    assert(!strcmp(trace, "stop w98cd wait\nclose w98cd wait\n"));
    assert(perform_action("cd/open/D"));
    trace[0] = 0;
    assert(perform_action("cd/open/E"));
    assert(strstr(trace, "stop w98cd wait\nclose w98cd wait\nopen E:") == trace);
    close_cd();
    /* Exercise the real queue and worker shutdown, with only the CD widget enabled. */
    assert(GetCurrentDirectoryA(sizeof(directory), directory));
    assert(strlen(directory) + 32 < sizeof(directory));
    strcat(directory, "\\cdshutdown-test");
    CreateDirectoryA(directory, NULL);
    sprintf(path, "%s\\WIDGETS.INI", directory);
    for (i = 0; i < 22; i++)
    {
        sprintf(row, "|%d|1|1|000000|FFFFFF|FFFFFF|100|2", i == 9);
        strcat(config, row);
    }
    assert(WritePrivateProfileStringA("Desktop", "Layout", config, path));
    trace[0] = 0;
    owner = 0;
    openedEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    playedEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    assert(openedEvent && playedEvent);
    extra_start(directory);
    assert(device_action("cd/open/D"));
    assert(WaitForSingleObject(openedEvent, 5000) == WAIT_OBJECT_0);
    assert(device_action("cd/play"));
    assert(WaitForSingleObject(playedEvent, 5000) == WAIT_OBJECT_0);
    extra_stop();
    assert(owner && owner != GetCurrentThreadId());
    assert(strlen(trace) >= 31);
    assert(strstr(trace, "stop w98cd wait\nclose w98cd wait\n"));
    assert(!cdopen);
    assert(!device_action("cd/play"));
    extra_stop();
    CloseHandle(openedEvent);
    CloseHandle(playedEvent);
    DeleteFileA(path);
    sprintf(path, "%s\\DISK.TMP", directory);
    DeleteFileA(path);
    RemoveDirectoryA(directory);
    puts("PASS: CD track wraparound, previous/next, invalid status, stop-before-close, drive changes, worker ownership, queued controls and idempotent shutdown");
    return 0;
}
