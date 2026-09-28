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
.\scripts\test-autotheme.ps1
```

Native test executables and scratch files stay in `build`. Test helpers operate
on isolated INI files rather than an installed desktop configuration.

The `build.ps1` script also builds diagnostic utilities and the standalone
`system.htm` page. Use `package-glass.ps1` for the Glass98 release package.
