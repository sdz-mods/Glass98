# Build and test Glass98

Run all commands below from the repository root, not from this directory.

## Host setup

Use Windows PowerShell 5.1 or later and Open Watcom 2.0 with both Win32 and
Win16 target libraries and the 64-bit Windows host tools. The tested compiler
reports June 23, 2026. Obtain it from the
[Open Watcom project](https://github.com/open-watcom/open-watcom-v2).

Set `WATCOM` to its installation root. A local `tools/ow` installation takes
precedence if present. Toolchains and generated outputs are excluded from Git.

```powershell
$env:WATCOM = 'C:\WATCOM'
.\scripts\check-toolchain.ps1
.\scripts\package-glass.ps1
.\scripts\audit-imports.ps1 -Glass
```

PowerShell execution policy may require launching with `-ExecutionPolicy Bypass`.
The scripts set compiler environment variables for their process. Paths with
spaces are supported. Output is `dist/Glass98` and `dist/Glass98.zip`.

## Package

The release contains six native programs: GADGETCTL, W98DATA, GLASSCTL,
GLASSPRF, G98SETUP and the Win16 RSRC16 resource helper. Five are PE executables
with subsystem version 4.0; RSRC16 is a Windows NE executable. Runtime source
availability notices are embedded as resources and included in the package.

`VERSION` is the release version source. Packaging substitutes it into both
HTML pages and VERSION.TXT. Keep the version in the package manual and
NOTICE.TXT in step with it.

Builds use the locally installed toolchain without downloading dependencies.
Do not run the installer on the build host; it targets Windows 98 SE.

## Automated checks

Node.js is needed for JScript logic checks; Python 3 for the native settings
and placement test harnesses. These are development tools, not runtime needs.
After building:

```powershell
node scripts/test-glass.cjs
python scripts/test-setup.py
python scripts/test-placement.py
.\scripts\test-layouts.ps1
.\scripts\test-cpuname.ps1
.\scripts\test-frequency.ps1
.\scripts\test-cpumeter.ps1
.\scripts\test-autotheme.ps1
.\scripts\test-cdshutdown.ps1
.\scripts\test-desktop-state.ps1
.\scripts\test-dosquiet.ps1
```

Native test executables and scratch files stay in `build`. Test helpers operate
on isolated INI files rather than an installed desktop configuration.

The `build.ps1` script also builds diagnostic utilities and the standalone
`system.htm` page. Use `package-glass.ps1` for the Glass98 release package.

## Optional offline CD database

The catalog contains only MusicBrainz CC0 core data. Download `mbdump.tar.bz2`
and `SHA256SUMS` from a dated directory at
https://data.metabrainz.org/pub/musicbrainz/data/fullexport/ . Keep the archive,
extracted tables and scratch SQLite database outside the source tree. Allow
ample temporary disk space for the compressed dump and selected raw tables.

Build with Python 3.11 or later, using the published checksum for that archive:

```powershell
python scripts/build-cd-database.py --archive C:\data\mbdump.tar.bz2 --sha256 CHECKSUM --work C:\data\cd-build --output build\CDMETA.DAT --snapshot YYYY-MM-DD
./scripts/test-cdcatalog.ps1
./scripts/package-glass.ps1 -CdDatabase build\CDMETA.DAT
```

Use a separate work directory for each source snapshot. The builder verifies
the archive, extracts an allowlist of core tables and emits `CDMETA.DAT` with a
JSON build manifest. The package checks that manifest and includes the CC0
notice. Without `-CdDatabase`, the package omits this optional component.

Catalog format: little-endian `G98CDDB1` header, entry count, index offset,
snapshot date (YYYYMMDD) and three reserved zero words. The header is 32 bytes.
Each payload contains a 32-bit track count, relative frame offsets and
NUL-terminated UTF-8 album, artist and track names. The sorted index contains
16-byte records: FNV-1a hash of track count/relative offsets, duration in frames,
payload offset and payload length. The native reader verifies all offsets
after the hash lookup, allows a two-frame lead-out difference, and bounds every
read. Files must be smaller than 2 GiB. Duplicate metadata is removed while
distinct matching releases remain selectable. No cover artwork is included.

Desktop state checks: run `scripts/test-desktop-state.ps1` to test CPU-frequency
cache validation and scheme command parsing without changing host appearance.
On a Windows 98 test machine, `test-winschemes.exe --exercise` additionally
applies a native Appearance scheme and restores the previous named scheme.

## CPU-counter VxD

`./scripts/build-vxd.ps1` builds a standalone Windows 98 test package in
`build/cpu-prototype`. The normal package build also builds and includes the
driver, offered as an optional component by setup.
Follow [the test instructions](../src/vxd/README.txt) for installation and use.
The test package includes `CPUCHECK` for controlled load/timer tests, `CPUSETUP`
for a standalone test installation, and `CPUDIAG` for read-only diagnosis of
an installed Glass98 copy. These utilities are not in the release package.

The driver uses Open Watcom's `win_vxd dynamic` LE target with a DDB exported
at ordinal 1. It has no CRT, imports no Win32 APIs, and uses resident code/data.
The build applies vmdisp9x's `fixlink -vxd32`, enables LE internal relocations,
and corrects Watcom's DDB export to a 32-bit entry. The resulting image is
registered as a boot-loaded VxD; the client opens the resident `G98CPU` device.
The driver uses `UNDEFINED_DEVICE_ID`: clients open it by name, and it exports
no numeric services. The build checks this to prevent conflicts with other VxDs.

The LE recipe was adapted from VOPL3. `src/vxd/include/vmm.h` and
`scripts/fixlink/fixlink.c` originate from
[JHRobotics/vmdisp9x](https://github.com/JHRobotics/vmdisp9x); their copyright
and license notices are retained. No VOPL3 audio implementation is included.

The installer records `[CPU] Method=VxD` or `Method=Legacy` in `WIDGETS.INI`.
VxD mode does not fall back to the registry counter if the driver is unavailable.
Telemetry closes its device handle when collection stops and takes a fresh
baseline when collection resumes, including after fullscreen DOS sessions.
`scripts/test-cpumeter.c` tests load calculations, protocol validation, counter
rollovers and stale-baseline rejection; run it with `scripts/test-cpumeter.ps1`.
