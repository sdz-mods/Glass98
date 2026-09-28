# Telemetry helper

`W98DATA.EXE` is a native process with a hidden message window for normal
Windows shutdown handling. `/register` starts it and adds its current-user
Run entry; `/unregister` removes that entry, stops it and removes `DATA.JS`.
A named mutex prevents duplicate sampler processes.

In the release build, CPU and RAM sampling runs once per second while System
or Performance is enabled. Disabling both stops core collection and removes
its snapshot. Optional device collectors publish `EXTRA.JS` from a worker
thread and run only when needed by enabled widgets or a manager discovery
request. Refresh settings control display updates; disk capacity queries also
follow their effective refresh interval.

The HTML desktop polls local snapshots once per second and renders each widget
at its configured interval, or the global override. Native telemetry does not
run inside Explorer. Core data older than ten seconds is treated as unavailable;
extra data expires after fifteen seconds. CPU frequency detection is attempted
only once per helper process, even if core collection is disabled and reenabled.

`DATA.JS` contains a timestamp and a pipe-delimited snapshot: CPU percentage
(-1 when unavailable), Windows memory load, physical total/available KiB,
page-file total/available KiB, virtual total/available KiB, CPU brand, vendor,
family, model, stepping, OS processor count, frequency in MHz and frequency kind
(1: base, 2: startup estimate, 3: TSC reference rate). The desktop displays
physical-memory usage, not the separate Windows memory-load value.

CPU usage comes from the Win9x system counter without extra averaging. CPUID
is guarded by an EFLAGS ID-bit test. Processor identification uses the brand
string when available, with vendor/signature and cache-based fallbacks.

Snapshot strings escape JavaScript metacharacters. Core snapshot strings also
replace control and non-ASCII characters. Completed temporary files are closed
before publication. Win9x has no ReplaceFile, so publication uses delete/move;
the desktop tolerates a missing snapshot and retries on its next poll.
