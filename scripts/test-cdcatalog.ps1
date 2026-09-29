$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
    & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-cdcatalog.exe' "$projectRoot\scripts\test-cdcatalog.c" "$projectRoot\src\bridge\cdcatalog.c" user32.lib advapi32.lib
    if ($LASTEXITCODE) { throw 'CD catalog test build failed' }
    & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-cdoptional.exe' "$projectRoot\scripts\test-cdinstall.c" user32.lib
    if ($LASTEXITCODE) { throw 'CD installer test build failed' }
    & python "$PSScriptRoot\test-cdcatalog.py"
    if ($LASTEXITCODE) { throw 'CD catalog tests failed' }
} finally { Pop-Location }
