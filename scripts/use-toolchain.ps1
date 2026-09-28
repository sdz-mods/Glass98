$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$localCompiler = Join-Path $projectRoot 'tools\ow'
if (Test-Path -LiteralPath (Join-Path $localCompiler 'binnt64\wcl386.exe')) {
    $compilerRoot = $localCompiler
} elseif ($env:WATCOM) {
    $compilerRoot = $env:WATCOM
} else {
    throw 'Install Open Watcom 2.0 and set WATCOM to its root, or place it in tools\ow.'
}
$compilerRoot = (Resolve-Path -LiteralPath $compilerRoot).Path
foreach ($file in @('binnt64\wcl386.exe','binnt64\wcl.exe','binnt64\wrc.exe','h\nt\windows.h')) {
    if (!(Test-Path -LiteralPath (Join-Path $compilerRoot $file))) {
        throw "Incomplete Open Watcom toolchain: $compilerRoot\$file"
    }
}
$env:WATCOM = $compilerRoot
$env:INCLUDE = "$compilerRoot\h;$compilerRoot\h\nt"
$env:PATH = "$compilerRoot\binnt64;$compilerRoot\binnt;$env:PATH"
