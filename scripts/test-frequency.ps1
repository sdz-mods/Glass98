$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
 & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-frequency.exe' "$projectRoot\scripts\test-frequency.c" "$projectRoot\src\bridge\telemetry.c" advapi32.lib
 if($LASTEXITCODE){throw 'Frequency test compile failed'}
 & .\test-frequency.exe
 if($LASTEXITCODE){throw 'Frequency cache test failed'}
} finally {Pop-Location}
