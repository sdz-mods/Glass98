$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
    & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-cdshutdown.exe' "$projectRoot\scripts\test-cdshutdown.c" "$projectRoot\src\bridge\addons.c" advapi32.lib winmm.lib gdi32.lib shell32.lib ole32.lib
    if ($LASTEXITCODE) { throw 'CD shutdown test build failed' }
    & .\test-cdshutdown.exe
    if ($LASTEXITCODE) { throw 'CD shutdown test failed' }
} finally { Pop-Location }
