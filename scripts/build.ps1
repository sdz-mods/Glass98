$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
$outputRoot = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
Push-Location $outputRoot
try {
    & wcl386.exe -q -bt=nt -l=nt -3r -os '-fe=gadgetctl.exe' "$projectRoot\src\gadgetctl\gadgetctl.c" ole32.lib shell32.lib shlwapi.lib
    if ($LASTEXITCODE -ne 0) { throw 'gadgetctl build failed' }
    & wcl386.exe -q -bt=nt -l=nt_win -3r -os '-fe=w98data.exe' "$projectRoot\src\bridge\datawriter.c" "$projectRoot\src\bridge\telemetry.c" advapi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'bridge build failed' }
    & wcl386.exe -q -bt=nt -l=nt -3r -os '-fe=probe.exe' "$projectRoot\src\bridge\probe.c" "$projectRoot\src\bridge\telemetry.c" advapi32.lib version.lib
    if ($LASTEXITCODE -ne 0) { throw 'probe build failed' }
    & wcl386.exe -q -bt=nt -l=nt_win -3r -os '-fe=guestpower.exe' "$projectRoot\src\gadgetctl\guestpower.c"
    if ($LASTEXITCODE -ne 0) { throw 'guest power helper build failed' }
    Copy-Item "$projectRoot\gadgets\system.htm" $outputRoot -Force
} finally { Pop-Location }
