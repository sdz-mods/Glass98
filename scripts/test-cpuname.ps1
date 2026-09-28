$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
 & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-cpuname.exe' "$projectRoot\scripts\test-cpuname.c"
 if($LASTEXITCODE){throw 'CPU name test compile failed'}
 & .\test-cpuname.exe
 if($LASTEXITCODE){throw 'CPU name test failed'}
} finally {Pop-Location}
