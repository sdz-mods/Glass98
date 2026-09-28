$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
$compilerRoot = $env:WATCOM
foreach ($relativePath in @('binnt64\wcc386.exe', 'binnt64\wcl386.exe', 'binnt64\wlink.exe', 'h\nt\windows.h', 'h\nt\shlobj.h')) {
    $resolvedFile = Join-Path $compilerRoot $relativePath
    if (-not (Test-Path -LiteralPath $resolvedFile -PathType Leaf)) {
        throw "Missing toolchain input: $resolvedFile"
    }
    Get-FileHash -LiteralPath $resolvedFile -Algorithm SHA256 | Select-Object Path, Hash
}
