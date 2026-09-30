$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
    & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-dosquiet.exe' "$projectRoot\scripts\test-dosquiet.c" user32.lib
    if ($LASTEXITCODE) { throw 'DOS quiet detector compile failed' }
    & .\test-dosquiet.exe
    if ($LASTEXITCODE) { throw 'DOS quiet detector tests failed' }
} finally { Pop-Location }
& (Join-Path $PSScriptRoot 'test-cdshutdown.ps1')
