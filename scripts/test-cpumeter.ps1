$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
$output = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    & wcl386.exe -q -bt=nt -l=nt "$projectRoot\scripts\test-cpumeter.c" '-fe=test-cpumeter.exe'
    if ($LASTEXITCODE) { throw 'CPU meter test build failed' }
    & .\test-cpumeter.exe
    if ($LASTEXITCODE) { throw 'CPU meter test failed' }
} finally { Pop-Location }
