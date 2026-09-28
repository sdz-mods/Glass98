$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
 $sources=@('rss.c','actions.c','themes.c','palette.c','wallimage.c') | ForEach-Object {Join-Path "$projectRoot\src\glassctl" $_}
 & wcl386 -q -bt=nt -l=nt -3r -os '-fe=test-layouts.exe' "$projectRoot\scripts\test-layouts.c" @sources oleaut32.lib gdi32.lib advapi32.lib comdlg32.lib shell32.lib ole32.lib wininet.lib winmm.lib wsock32.lib
 if($LASTEXITCODE){throw 'Layout test compile failed'}
 & .\test-layouts.exe (Join-Path $PWD 'test-layouts.ini')
 if($LASTEXITCODE){throw 'Layout tests failed'}
} finally {Pop-Location}
