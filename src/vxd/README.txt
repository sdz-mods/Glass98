Glass98 CPU driver diagnostics
=============================

Standalone Windows 98 diagnostic. For normal Glass98 use, choose the VxD
option in INSTALL.BAT instead. A restart is required to load or remove the
driver. Do not run this standalone setup over a normal Glass98 installation;
both use the same driver registration.

INSTALL
Copy this whole directory to a permanent local folder, e.g. C:\G98TEST.
In a Windows MS-DOS prompt, change to that folder and run:

    CPUSETUP install

Restart Windows, then return to this folder and run:

    CPUCHECK

The test takes 12 seconds. It prints CPU usage and writes CPUVXD.LOG in the
current directory, replacing the previous log. Keep the window open to read
the results. To compare timer periods and controlled CPU loads:

    CPUCHECK 5 0
    CPUCHECK 1 0
    CPUCHECK 5 50
    CPUCHECK 5 100
    CPUCHECK 0 0

First argument: requested multimedia timer period in ms; 0 makes no request.
Second argument: busy-loop duty percentage, from 0 to 100. The load is
intentional and lasts only for this test. Timer requests are released on
normal exit. Other applications can still hold their own timer requests.
The log includes both the driver estimate and Windows' registry CPU counter.
Save each log under another filename before running the next test.

REMOVE
Run CPUSETUP remove and restart Windows. The test folder can then be deleted.
Setup only registers/removes HKLM\System\CurrentControlSet\Services\VxD\G98CPU.
It leaves Glass98's installation and settings unchanged. If Windows cannot
start normally, start Safe Mode and run CPUSETUP remove, then restart.

METHOD AND LIMITS
The driver reads the VMM system-thread execution counter and the VTD real-time
counter on request. The client estimates busy time as elapsed time minus
system-thread time. The driver sets no timer resolution, installs no hooks
and performs no background polling. Timer changes made by other applications
are not altered. Counter reads are sequential, so small rounding/skew near
zero is normal; the displayed percentage is bounded to 0..100.

The Processes widget additionally enables bounded thread-lifecycle accounting.
Thread_Init records ownership and Thread_Not_Executeable retains final CPU time,
including workers that begin and finish between reads. No scheduler hooks are
installed. Accounting stops when the owning client closes, disables the widget
or pauses for fullscreen DOS. Resuming starts a fresh interval.

The process protocol uses BEGIN (2), BIND (3), READ (4), STOP (5) and STATUS (6).
BEGIN seeds live thread handles; the Win98 client validates TDB ownership links
and supplies PID mappings through BIND. New threads acquire ownership at init.
READ returns cumulative process counters, generation IDs and VTD elapsed time.
Storage is bounded to 512 threads and 128 process groups (about 16.5 KiB). Capacity
exhaustion reports unavailable data rather than an incomplete CPU ranking.
Only one accounting client is supported; independent total-CPU readers remain
compatible with the original READ (1) protocol.

The driver is opened by name and exports no numeric services. It uses UNDEFINED_DEVICE_ID
to avoid claiming a fixed device number used by another installed VxD.

DIAGNOSING AN INSTALLED COPY
CPUDIAG.EXE reads C:\Glass98 settings, driver registration, installed file
checksums and raw counters. It saves CPUDIAG.TXT beside itself, replacing
the previous log. Run it from a writable folder. It does not change settings,
request a timer period, install a driver or generate a deliberate CPU load.
The log records device-open and IOCTL errors, and compares raw counters with
the CPU calculation used by the widget. No CPUSETUP command is needed for
this diagnostic.

LICENSES
Glass98 code: MIT, see LICENSE.txt.
VMM header: Copyright 2023 Jaroslav Hensl <emulator@emulace.cz>, MIT.
The header and build-time fixlink utility originate from JHRobotics/vmdisp9x.
See VMDISP9X.TXT. The fixlink source retains its MIT No Attribution license.
The diagnostic/setup executables use Open Watcom runtime libraries; see
NOTICE.txt and WATCOM.TXT for copyright, license and source availability.
