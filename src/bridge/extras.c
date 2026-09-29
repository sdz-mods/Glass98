/* Optional hardware collectors. Runs separately from the CPU/RAM sampler. */
#include <windows.h>
#include <mmsystem.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extras.h"
#include "addons.h"
static char root[MAX_PATH], ini[MAX_PATH];
static HANDLE stopEvent, wakeEvent, worker;
static LONG inspectRequested;
static CRITICAL_SECTION actionLock;
static char queuedAction[128];
static int shuttingDown;
static HMODULE ipmodule;
static DWORD (WINAPI *adapters)(PIP_ADAPTER_INFO, PULONG);
static DWORD (WINAPI *ifentry)(PMIB_IFROW);
static DWORD (WINAPI *netparams)(PFIXED_INFO_W2KSP1, PULONG);
static int cdopen = 0;
static int refresh_seconds(int panel, int *enabled);
/* CD commands and queries belong to the collector thread, including shutdown. */
static void close_cd(void)
{
    if (cdopen)
    {
        mciSendStringA("stop w98cd wait", NULL, 0, NULL);
        mciSendStringA("close w98cd wait", NULL, 0, NULL);
        cdopen = 0;
    }
}
static char actionStatus[160] = "";
void js_string(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char*)s;
    fputc('\'', f);
    for (; *p; p++)
    {
        if (*p < 32 || *p >= 127)
            fprintf(f, "\\x%02x", *p);
        else
        {
            if (*p == '\'' || *p == '\\' || *p == '<')
                fprintf(f, "\\x%02x", *p);
            else
                fputc(*p, f);
        }
    }
    fputc('\'', f);
}
static void named(FILE *f, const char *key, const char *value)
{
    fprintf(f, "%s:", key);
    js_string(f, value);
    fputc(',', f);
}
static double widebytes(ULARGE_INTEGER n)
{
    return (double)n.HighPart * 4294967296.0 + n.LowPart;
}
/* Query the device driver description; never infer marketing names from PCI IDs. */
static void hardware(FILE *f)
{
    DISPLAY_DEVICEA dd;
    DEVMODEA mode;
    HDC dc;
    OSVERSIONINFOA os;
    BOOL (WINAPI * enumerate)(LPCSTR, DWORD, PDISPLAY_DEVICEA, DWORD);
    char gpu[160] = "Display adapter unavailable", version[100];
    DWORD i;
    HKEY k;
    DWORD size, type;
    enumerate = (void*)GetProcAddress(GetModuleHandleA("USER32.DLL"), "EnumDisplayDevicesA");
    if (enumerate)
    {
        for (i = 0; i < 16; i++)
        {
            memset(&dd, 0, sizeof(dd));
            dd.cb = sizeof(dd);
            if (!enumerate(NULL, i, &dd, 0))
                break;
            if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                lstrcpynA(gpu, dd.DeviceString, sizeof(gpu));
                break;
            }
        }
    }
    if (!strcmp(gpu, "Display adapter unavailable") &&
            !RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\Class\\Display\\0000", 0, KEY_READ, &k))
    {
        size = sizeof(gpu);
        type = REG_SZ;
        if (RegQueryValueExA(k, "DriverDesc", 0, &type, (BYTE*)gpu, &size))
            strcpy(gpu, "Display adapter unavailable");
        gpu[sizeof(gpu) - 1] = 0;
        RegCloseKey(k);
    }
    memset(&mode, 0, sizeof(mode));
    mode.dmSize = sizeof(mode);
    EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode);
    dc = GetDC(NULL);
    memset(&os, 0, sizeof(os));
    os.dwOSVersionInfoSize = sizeof(os);
    GetVersionExA(&os);
    sprintf(version, "Windows %s %lu.%lu.%lu", os.dwMinorVersion == 10 ? "98 SE" : "9x", os.dwMajorVersion,
            os.dwMinorVersion, os.dwBuildNumber & 65535);
    fputs("var hardware={", f);
    named(f, "gpu", gpu);
    named(f, "os", version);
    fprintf(f, "width:%d,height:%d,bpp:%d,hz:%lu,uptime:%lu};\n", GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN), GetDeviceCaps(dc, BITSPIXEL)*GetDeviceCaps(dc, PLANES), mode.dmDisplayFrequency,
            GetTickCount() / 1000);
    ReleaseDC(NULL, dc);
}
static void power_status(FILE *f)
{
    SYSTEM_POWER_STATUS power;
    memset(&power, 255, sizeof(power));
    if (!GetSystemPowerStatus(&power))
        memset(&power, 255, sizeof(power));
    fprintf(f, "var battery={ac:%u,flags:%u,percent:%u,seconds:%lu};\n", power.ACLineStatus, power.BatteryFlag,
            power.BatteryLifePercent, power.BatteryLifeTime);
}
static void disks(FILE *f, int query)
{
    DWORD mask = GetLogicalDrives(), i, type, serial, flags, maxcomp;
    char path[] = "C:\\", letter[2], label[80], fs[32], selection[80];
    ULARGE_INTEGER available, total, freebytes;
    int first = 1, ok;
    GetPrivateProfileStringA("Options", "drives", "C", selection, sizeof(selection), ini);
    fputs("var drives=[", f);
    for (i = 0; i < 26; i++)
        if (mask & (1UL << i))
        {
            path[0] = (char)('A' + i);
            type = GetDriveTypeA(path);
            letter[0] = path[0];
            letter[1] = 0;
            /* Do not spin floppies by default. Network mappings may be unavailable. */
            ok = 0;
            label[0] = 0;
            fs[0] = 0;
            memset(&total, 0, sizeof(total));
            memset(&freebytes, 0, sizeof(freebytes));
            if (query && (strchr(selection, path[0]) || (!strcmp(selection, "*") && i >= 2)) && type != DRIVE_REMOTE)
            {
                ok = GetDiskFreeSpaceExA(path, &available, &total, &freebytes);
                if (ok)
                    GetVolumeInformationA(path, label, sizeof(label), &serial, &maxcomp, &flags, fs, sizeof(fs));
            }
            if (!first)
                fputc(',', f);
            first = 0;
            fputc('{', f);
            named(f, "id", letter);
            named(f, "label", label);
            named(f, "fs", fs);
            fprintf(f, "type:%lu,ready:%d,total:%.0f,free:%.0f}", type, ok, widebytes(total), widebytes(freebytes));
        }
    fputs("];\n", f);
}
typedef struct
{
    char id[260];
    DWORD in, out, tick;
    double totalIn, totalOut;
    int used;
} NETLAST;
static NETLAST previous[32];
static void network(FILE *f)
{
    ULONG size = 0;
    PIP_ADAPTER_INFO list, a;
    MIB_IFROW row;
    FIXED_INFO_W2KSP1 *info;
    char host[132] = "", identity[260];
    int first = 1, n, j;
    DWORD tick = GetTickCount(), din, dout, elapsed;
    double rx, tx, limit;
    NETLAST *last;
    if (netparams)
    {
        netparams(NULL, &size);
        if (size && size < 65536)
        {
            info = malloc(size);
            if (info)
            {
                if (!netparams(info, &size))
                    lstrcpynA(host, info->HostName, sizeof(host));
                free(info);
            }
        }
    }
    fputs("var hostName=", f);
    js_string(f, host);
    fputs(";var networks=[", f);
    size = 0;
    if (adapters)
        adapters(NULL, &size);
    list = size && size < 262144 ? malloc(size) : NULL;
    if (list && !adapters(list, &size))
        for (a = list, n = 0; a && n < 32; a = a->Next, n++)
        {
            lstrcpynA(identity, a->AdapterName, sizeof(identity));
            if (!*identity)
            {
                if (a->AddressLength >= 6)
                    sprintf(identity, "mac-%02X%02X%02X%02X%02X%02X", a->Address[0], a->Address[1], a->Address[2], a->Address[3],
                            a->Address[4], a->Address[5]);
                else
                    sprintf(identity, "if-%lu", a->Index);
            }
            memset(&row, 0, sizeof(row));
            row.dwIndex = a->Index;
            rx = -1;
            tx = -1;
            last = NULL;
            for (j = 0; j < 32; j++)
                if (previous[j].used && !strcmp(previous[j].id, identity))
                {
                    last = &previous[j];
                    break;
                }
            if (!last)
                for (j = 0; j < 32; j++)
                    if (!previous[j].used)
                    {
                        last = &previous[j];
                        memset(last, 0, sizeof(*last));
                        lstrcpynA(last->id, identity, sizeof(last->id));
                        last->used = 1;
                        break;
                    }
            if (ifentry && !ifentry(&row) && last)
            {
                if (last->tick)
                {
                    elapsed = tick - last->tick;
                    din = row.dwInOctets - last->in;
                    dout = row.dwOutOctets - last->out;
                    limit = row.dwSpeed ? (double)row.dwSpeed / 8.0 * (elapsed / 1000.0) * 1.5 + 65536 : 1073741824.0;
                    if (elapsed && din <= limit && dout <= limit)
                    {
                        rx = din * 1000.0 / elapsed;
                        tx = dout * 1000.0 / elapsed;
                        last->totalIn += din;
                        last->totalOut += dout;
                    }
                }
                last->in = row.dwInOctets;
                last->out = row.dwOutOctets;
                last->tick = tick;
            }
            if (!first)
                fputc(',', f);
            first = 0;
            fputc('{', f);
            named(f, "id", identity);
            named(f, "name", a->Description);
            named(f, "ip", a->IpAddressList.IpAddress.String);
            named(f, "gateway", a->GatewayList.IpAddress.String);
            fprintf(f, "rx:%.0f,tx:%.0f,totalIn:%.0f,totalOut:%.0f,admin:%lu,state:%lu,speed:%lu}", rx, tx,
                    last ? last->totalIn : 0, last ? last->totalOut : 0, row.dwAdminStatus, row.dwOperStatus, row.dwSpeed);
        }
    if (list)
        free(list);
    fputs("];\n", f);
}
static int mixercontrol(int channel, int mute, int set, int value)
{
    HMIXER mixer;
    MIXERLINE line;
    MIXERLINECONTROLS controls;
    MIXERCONTROL control;
    MIXERCONTROLDETAILS details;
    MIXERCONTROLDETAILS_UNSIGNED values[32];
    DWORD types[] = {MIXERLINE_COMPONENTTYPE_DST_SPEAKERS, MIXERLINE_COMPONENTTYPE_SRC_WAVEOUT, MIXERLINE_COMPONENTTYPE_SRC_COMPACTDISC, MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER};
    DWORD channels, i;
    int result = -1;
    if (channel < 0 || channel > 3 || mixerOpen(&mixer, 0, 0, 0, 0))
        return -1;
    memset(&line, 0, sizeof(line));
    line.cbStruct = sizeof(line);
    line.dwComponentType = types[channel];
    if (mixerGetLineInfoA((HMIXEROBJ)mixer, &line, MIXER_GETLINEINFOF_COMPONENTTYPE))
        goto done;
    memset(&controls, 0, sizeof(controls));
    memset(&control, 0, sizeof(control));
    controls.cbStruct = sizeof(controls);
    controls.dwLineID = line.dwLineID;
    controls.dwControlType = mute ? MIXERCONTROL_CONTROLTYPE_MUTE : MIXERCONTROL_CONTROLTYPE_VOLUME;
    controls.cControls = 1;
    controls.cbmxctrl = sizeof(control);
    controls.pamxctrl = &control;
    if (mixerGetLineControlsA((HMIXEROBJ)mixer, &controls, MIXER_GETLINECONTROLSF_ONEBYTYPE))
        goto done;
    channels = control.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM ? 1 : line.cChannels;
    if (!channels || channels > 32 || control.cMultipleItems)
        goto done;
    memset(&details, 0, sizeof(details));
    details.cbStruct = sizeof(details);
    details.dwControlID = control.dwControlID;
    details.cChannels = channels;
    details.cbDetails = sizeof(values[0]);
    details.paDetails = values;
    if (set)
    {
        for (i = 0; i < channels; i++)
            values[i].dwValue = mute ? value : control.Bounds.dwMinimum + (DWORD)((control.Bounds.dwMaximum -
                                control.Bounds.dwMinimum) * (value / 100.0));
        if (mixerSetControlDetails((HMIXEROBJ)mixer, &details, MIXER_SETCONTROLDETAILSF_VALUE))
            goto done;
    }
    if (mixerGetControlDetailsA((HMIXEROBJ)mixer, &details, MIXER_GETCONTROLDETAILSF_VALUE))
        goto done;
    result = mute ? (int)values[0].dwValue : control.Bounds.dwMaximum > control.Bounds.dwMinimum ? (int)((
                 values[0].dwValue - control.Bounds.dwMinimum) * 100.0 / (control.Bounds.dwMaximum - control.Bounds.dwMinimum)) : 0;
done:
    mixerClose(mixer);
    return result;
}
static DWORD winampmsg(HWND w, UINT msg, WPARAM wp, LPARAM lp, DWORD fallback)
{
    DWORD result = fallback;
    if (!SendMessageTimeoutA(w, msg, wp, lp, SMTO_ABORTIFHUNG, 300, &result))
        return fallback;
    return result;
}
static void devices(FILE *f)
{
    int winampEnabled, mixerEnabled, cdEnabled;
    HWND winamp;
    char title[256] = "Winamp is not running", mode[80] = "No audio CD opened", trackText[32] = "", totalText[32] = "";
    DWORD state = 0;
    int i;
    refresh_seconds(9, &cdEnabled);
    refresh_seconds(8, &winampEnabled);
    refresh_seconds(7, &mixerEnabled);
    winamp = winampEnabled ? FindWindowA("Winamp v1.x", NULL) : NULL;
    if (winamp)
    {
        GetWindowTextA(winamp, title, sizeof(title));
        state = winampmsg(winamp, WM_USER, 0, 104, 0);
    }
    fputs("var winamp={", f);
    named(f, "title", title);
    fprintf(f, "present:%d,state:%lu,volume:%lu};\n", winamp != NULL, state, winamp ? winampmsg(winamp, WM_USER,
            (WPARAM) - 666, 122, 0) * 100 / 255 : 0);
    fputs("var mixer=[", f);
    for (i = 0; i < 4; i++)
    {
        if (i)
            fputc(',', f);
        fprintf(f, "{volume:%d,mute:%d}", mixerEnabled ? mixercontrol(i, 0, 0, 0) : -1, mixerEnabled ? mixercontrol(i, 1, 0,
                0) : -1);
    }
    fputs("];\n", f);
    if (cdEnabled && cdopen && mciSendStringA("status w98cd mode", mode, sizeof(mode), NULL))
        strcpy(mode, "Audio CD unavailable");
    fputs("var cdState=", f);
    js_string(f, mode);
    if (cdEnabled && cdopen)
    {
        mciSendStringA("status w98cd current track", trackText, sizeof(trackText), NULL);
        mciSendStringA("status w98cd number of tracks", totalText, sizeof(totalText), NULL);
    }
    fprintf(f, ";var cdTrack=%d;var cdTracks=%d;", atoi(trackText), atoi(totalText));
    fputs("var actionStatus=", f);
    js_string(f, actionStatus);
    fputs(";\n", f);
}
static int perform_action(const char *s)
{
    int channel, value, mute;
    char tail, command[100], mode[40], drive;
    HWND w;
    DWORD error;
    unsigned int track;
    if (sscanf(s, "volume/%d/%d/%d%c", &channel, &mute, &value, &tail) == 3 && channel >= 0 && channel < 4 && mute >= 0 &&
            mute <= 1 && value >= 0 && value <= (mute ? 1 : 100))
    {
        if (mixercontrol(channel, mute, 1, value) >= 0)
        {
            actionStatus[0] = 0;
            return 1;
        }
        strcpy(actionStatus, "Mixer control unavailable");
        return 0;
    }
    if (!strncmp(s, "winamp/", 7))
    {
        w = FindWindowA("Winamp v1.x", NULL);
        if (!w)
        {
            strcpy(actionStatus, "Winamp is not running");
            return 0;
        }
        if (!strcmp(s + 7, "play"))
            winampmsg(w, WM_COMMAND, 40045, 0, 0);
        else if (!strcmp(s + 7, "pause"))
            winampmsg(w, WM_COMMAND, 40046, 0, 0);
        else if (!strcmp(s + 7, "stop"))
            winampmsg(w, WM_COMMAND, 40047, 0, 0);
        else if (!strcmp(s + 7, "next"))
            winampmsg(w, WM_COMMAND, 40048, 0, 0);
        else if (!strcmp(s + 7, "prev"))
            winampmsg(w, WM_COMMAND, 40044, 0, 0);
        else if (sscanf(s + 7, "volume/%d%c", &value, &tail) == 1 && value >= 0 && value <= 100)
            winampmsg(w, WM_USER, value * 255 / 100, 122, 0);
        else
            return 0;
        actionStatus[0] = 0;
        return 1;
    }
    if (!strncmp(s, "cd/", 3))
    {
        if (sscanf(s, "cd/open/%c%c", &drive, &tail) == 1 && drive >= 'A' && drive <= 'Z')
        {
            char path[4];
            sprintf(path, "%c:\\", drive);
            if (GetDriveTypeA(path) != DRIVE_CDROM)
                return 0;
            close_cd();
            sprintf(command, "open %c: type cdaudio alias w98cd shareable", drive);
            error = mciSendStringA(command, NULL, 0, NULL);
            if (!error)
            {
                cdopen = 1;
                mciSendStringA("set w98cd time format tmsf", NULL, 0, NULL);
                actionStatus[0] = 0;
                return 1;
            }
        }
        else if (cdopen)
        {
            if (!strcmp(s + 3, "play"))
                strcpy(command, "play w98cd");
            else if (!strcmp(s + 3, "pause"))
                strcpy(command, "pause w98cd");
            else if (!strcmp(s + 3, "stop"))
                strcpy(command, "stop w98cd");
            else if (!strcmp(s + 3, "eject"))
                strcpy(command, "set w98cd door open");
            else if (!strcmp(s + 3, "next"))
            {
                if (mciSendStringA("status w98cd current track", mode, sizeof(mode), NULL))
                    return 0;
                track = atoi(mode) + 1;
                sprintf(command, "play w98cd from %u", track);
            }
            else
                return 0;
            error = mciSendStringA(command, NULL, 0, NULL);
            if (!error)
            {
                actionStatus[0] = 0;
                return 1;
            }
        }
        else
        {
            strcpy(actionStatus, "Select an audio CD drive first");
            return 0;
        }
        mciGetErrorStringA(error, actionStatus, sizeof(actionStatus));
        return 0;
    }
    return 0;
}
static int refresh_seconds(int panel, int *enabled)
{
    char config[4096], *p;
    int i, global;
    GetPrivateProfileStringA("Desktop", "Layout", "", config, sizeof(config), ini);
    p = config;
    *enabled = panel == 0;
    if (strncmp(p, "2|", 2) && strncmp(p, "3|", 2))
        return 2;
    for (i = 0; i < 5 + panel * 8; i++)
    {
        p = strchr(p, '|');
        if (!p)
            return 2;
        p++;
    }
    *enabled = panel != 5 && atoi(p) != 0;
    for (i = 0; i < 7; i++)
    {
        p = strchr(p, '|');
        if (!p)
            return 2;
        p++;
    }
    i = atoi(p);
    global = GetPrivateProfileIntA("Options", "globalSeconds", 0, ini);
    if (global > 0 && global <= 3600)
        i = global;
    return i < 1 ? 1 : i > 3600 ? 3600 : i;
}
int extra_core_enabled(void)
{
    int system, graphs;
    refresh_seconds(0, &system);
    refresh_seconds(12, &graphs);
    return system || graphs;
}
void extra_refresh(void)
{
    if (wakeEvent)
        SetEvent(wakeEvent);
}
int device_action(const char *s)
{
    int result;
    if (!strcmp(s, "inspect"))
    {
        InterlockedExchange(&inspectRequested, 1);
        extra_refresh();
        return 1;
    }
    if (shuttingDown || strlen(s) >= sizeof(queuedAction))
        return 0;
    EnterCriticalSection(&actionLock);
    result = !queuedAction[0];
    if (result)
        strcpy(queuedAction, s);
    LeaveCriticalSection(&actionLock);
    extra_refresh();
    return result;
}
static DWORD WINAPI collect(void *unused)
{
    char destination[MAX_PATH], temporary[MAX_PATH], cachepath[MAX_PATH], cache[16384], action[128];
    char selection[80], lastSelection[80] = "", layout[4096], lastLayout[4096] = "";
    FILE *f, *diskfile;
    DWORD lastDisk = 0, waitResult;
    int interval, enabled[22], i, inspect, changed, active, lastEnabled = -1;
    size_t bytes;
    FILETIME ft;
    double now;
    HANDLE events[2];
    (void)unused;
    sprintf(destination, "%s\\EXTRA.JS", root);
    sprintf(temporary, "%s\\EXTRA.TMP", root);
    sprintf(cachepath, "%s\\DISK.TMP", root);
    strcpy(cache, "var drives=[];\n");
    events[0] = stopEvent;
    events[1] = wakeEvent;
    ipmodule = LoadLibraryA("IPHLPAPI.DLL");
    if (ipmodule)
    {
        adapters = (void*)GetProcAddress(ipmodule, "GetAdaptersInfo");
        ifentry = (void*)GetProcAddress(ipmodule, "GetIfEntry");
        netparams = (void*)GetProcAddress(ipmodule, "GetNetworkParams");
    }
    do
    {
        EnterCriticalSection(&actionLock);
        strcpy(action, queuedAction);
        queuedAction[0] = 0;
        LeaveCriticalSection(&actionLock);
        if (*action)
            perform_action(action);
        inspect = InterlockedExchange(&inspectRequested, 0);
        for (i = 0; i < 22; i++)
            refresh_seconds(i, &enabled[i]);
        enabled[21] = 0; /* Reserved layout slot; never enable a collector. */
        GetPrivateProfileStringA("Desktop", "Layout", "", layout, sizeof(layout), ini);
        changed = strcmp(layout, lastLayout) != 0;
        if (changed)
            strcpy(lastLayout, layout);
        active = enabled[0] || enabled[1] || enabled[2] || enabled[6] || enabled[7] || enabled[8] || enabled[9] ||
                 enabled[12] || enabled[13] || enabled[14] || enabled[17] || enabled[18];
        GetPrivateProfileStringA("Options", "drives", "C", selection, sizeof(selection), ini);
        interval = refresh_seconds(1, &enabled[1]);
        if ((enabled[1] && (!lastDisk || strcmp(selection, lastSelection) || lastEnabled != 1 ||
                            GetTickCount() - lastDisk >= (DWORD)interval * 1000)) || inspect || (enabled[9] && changed))
        {
            diskfile = fopen(cachepath, "wb");
            if (diskfile)
            {
                disks(diskfile, enabled[1]);
                fclose(diskfile);
                diskfile = fopen(cachepath, "rb");
                if (diskfile)
                {
                    bytes = fread(cache, 1, sizeof(cache) - 1, diskfile);
                    cache[bytes] = 0;
                    fclose(diskfile);
                }
            }
            lastDisk = GetTickCount();
            strcpy(lastSelection, selection);
        }
        lastEnabled = enabled[1];
        addons_idle(enabled);
        if (!enabled[9])
            close_cd();
        if (active || inspect || changed)
        {
            f = fopen(temporary, "wb");
            if (f)
            {
                GetSystemTimeAsFileTime(&ft);
                now = ((double)ft.dwHighDateTime * 4294967296.0 + ft.dwLowDateTime) / 10000.0 - 11644473600000.0;
                fprintf(f, "var extraTime=%.0f;\n", now);
                if (inspect)
                    fprintf(f, "var inventoryTime=%.0f;\n", now);
                if (enabled[0])
                    hardware(f);
                else
                    fputs("var hardware={};\n", f);
                if (enabled[6])
                    power_status(f);
                else
                    fputs("var battery={};\n", f);
                if (enabled[2] || enabled[12] || enabled[13] || inspect)
                    network(f);
                else
                {
                    for (i = 0; i < 32; i++)
                        previous[i].tick = 0;
                    fputs("var hostName='';var networks=[];\n", f);
                }
                if (enabled[1] || enabled[9] || inspect)
                    fputs(cache, f);
                else
                    fputs("var drives=[];\n", f);
                devices(f);
                addons_collect(f, root, ini, enabled);
                fprintf(f,
                        "var collecting={core:%d,display:%d,disks:%d,network:%d,battery:%d,mixer:%d,winamp:%d,cd:%d,fileActivity:%d,resources:%d,recent:%d,media:%d,vu:%d};\n",
                        enabled[0] || enabled[12], enabled[0], enabled[1], enabled[2] || enabled[12] ||
                        enabled[13], enabled[6], enabled[7], enabled[8], enabled[9], enabled[12], enabled[14], enabled[17], enabled[18], 0);
                if (!ferror(f))
                {
                    fclose(f);
                    if (DeleteFileA(destination) || GetLastError() == ERROR_FILE_NOT_FOUND)
                        MoveFileA(temporary, destination);
                }
                else
                    fclose(f);
            }
        }
        waitResult = WaitForMultipleObjects(2, events, FALSE, 1000);
    }
    while (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_FAILED);
    addons_stop();
    close_cd();
    if (ipmodule)
        FreeLibrary(ipmodule);
    return 0;
}
void extra_start(const char *directory)
{
    DWORD id;
    InitializeCriticalSection(&actionLock);
    queuedAction[0] = 0;
    shuttingDown = 0;
    lstrcpynA(root, directory, sizeof(root));
    sprintf(ini, "%s\\WIDGETS.INI", root);
    stopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    wakeEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (stopEvent && wakeEvent)
        worker = CreateThread(NULL, 0, collect, NULL, 0, &id);
}
void extra_stop(void)
{
    if (shuttingDown)
        return;
    shuttingDown = 1;
    if (stopEvent)
        SetEvent(stopEvent);
    if (worker)
    {
        WaitForSingleObject(worker, 4000);
        CloseHandle(worker);
    }/* process exit releases events if a device call is still completing */
}
