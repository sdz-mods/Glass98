$ErrorActionPreference='Stop'
& "$PSScriptRoot\use-toolchain.ps1"
Push-Location (Join-Path (Split-Path $PSScriptRoot -Parent) 'build')
try {
    & wcl386 -q -bt=nt -l=nt -3r -os "$PSScriptRoot\test-ntstats.c" '-fe=test-ntstats.exe'
    if($LASTEXITCODE){throw 'NT counter tests failed to compile'}
    & .\test-ntstats.exe
    if($LASTEXITCODE){throw 'NT counter tests failed'}
    & wcl386 -q -bt=nt -l=nt -3r -os "$PSScriptRoot\probe-nt.c" "$PSScriptRoot\..\src\bridge\telemetry.c" "$PSScriptRoot\..\src\bridge\processes.c" '-fe=PROBENT.EXE' advapi32.lib winmm.lib
    if($LASTEXITCODE){throw 'XP VM probe failed to compile'}
} finally {Pop-Location}
