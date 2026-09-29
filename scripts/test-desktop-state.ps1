$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
    foreach ($test in @('cpucache', 'winschemes')) {
        & wcl386 -q -bt=nt -l=nt -3r -os "-fe=test-$test.exe" "$projectRoot\scripts\test-$test.c" advapi32.lib user32.lib
        if ($LASTEXITCODE) { throw "$test compile failed" }
        & ".\test-$test.exe"
        if ($LASTEXITCODE) { throw "$test checks failed" }
    }
} finally { Pop-Location }
