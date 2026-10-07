$ErrorActionPreference = 'Stop'
& "$PSScriptRoot\use-toolchain.ps1"
$output = Join-Path (Split-Path $PSScriptRoot -Parent) 'build\process-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    & wcl386 -q -bt=nt -l=nt -3r -os "$PSScriptRoot\test-processes.c" '-fe=test-processes.exe'
    if ($LASTEXITCODE) { throw 'Process accounting test build failed' }
    & .\test-processes.exe
    if ($LASTEXITCODE) { throw 'Process accounting test failed' }
    & wcl386 -q -bt=nt -l=nt -3r -os "$PSScriptRoot\probe-processes.c" '-fe=PROCCHECK.EXE' winmm.lib
    if ($LASTEXITCODE) { throw 'Process VM probe build failed' }
} finally { Pop-Location }
