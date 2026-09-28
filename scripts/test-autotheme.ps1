$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
Push-Location (Join-Path $projectRoot 'build')
try {
    foreach($test in @('themes','autotheme')){
        & wcl386 -q -bt=nt -l=nt -3r -os "-fe=test-$test.exe" "$projectRoot\scripts\test-$test.c" "$projectRoot\src\glassctl\themes.c" "$projectRoot\src\glassctl\palette.c" "$projectRoot\src\glassctl\wallimage.c" ole32.lib oleaut32.lib gdi32.lib
        if($LASTEXITCODE){throw "$test compile failed"}
        & ".\test-$test.exe"
        if($LASTEXITCODE){throw "$test assertions failed"}
    }
} finally {Pop-Location}
