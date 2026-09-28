param([switch]$Glass)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
$dumper = Join-Path $env:WATCOM 'binnt64\wdump.exe'
$allowed = @('KERNEL32.DLL','USER32.DLL','ADVAPI32.DLL','OLE32.DLL','OLEAUT32.DLL','SHELL32.DLL','SHLWAPI.DLL','GDI32.DLL','COMDLG32.DLL','WINMM.DLL','WININET.DLL','WSOCK32.DLL')
$names=@('gadgetctl.exe','w98data.exe')
if($Glass){$names+='glassctl.exe';$names+='glassprf.exe';$names+='g98setup.exe'}
foreach ($name in $names) {
    $dump = (& $dumper -e (Join-Path $projectRoot "build\$name") | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $name" }
    $imports = @([regex]::Matches($dump,'DLL name = <([^>]+)>') | ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } | Sort-Object -Unique)
    if (-not $imports.Count) { throw "No import table found in $name" }
    foreach ($dll in $imports) { if ($dll -notin $allowed) { throw "$name has unexpected dependency $dll" } }
    if ($dump -notmatch 'subsystem major version number\s+=\s+0004H' -or $dump -notmatch 'subsystem minor version number\s+=\s+0000H') {
        throw "$name does not target subsystem 4.0"
    }
    "$name : subsystem 4.0; imports $($imports -join ', ')"
}
if($Glass){
    $dump=(& $dumper -e (Join-Path $projectRoot 'build\rsrc16.exe') | Out-String)
    if($LASTEXITCODE -or $dump -notmatch 'target OS.*=\s+02H' -or $dump -notmatch 'expected Windows version.*=\s+0300H') { throw 'Resource helper is not a Win16 Windows executable' }
    $modules=[regex]::Match($dump,'(?s)Module Reference Table\s*=+\s*(.*?)\s*Nonresident Names Table').Groups[1].Value.Trim()
    if(($modules -split '\s+' | Sort-Object) -join ',' -ne 'KERNEL,USER'){throw 'Unexpected Win16 imports'}
    'rsrc16.exe : Win16 Windows 3.0 NE; imports USER, KERNEL'
}
