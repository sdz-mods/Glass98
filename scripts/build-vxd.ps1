# Open Watcom LE recipe adapted from VOPL3, using JHRobotics' fixlink.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'use-toolchain.ps1')
$output = Join-Path $projectRoot 'build\vxd'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    & wcl386.exe -q -bt=nt -l=nt "$projectRoot\scripts\fixlink\fixlink.c" '-fe=fixlink.exe'
    if ($LASTEXITCODE) { throw 'fixlink build failed' }
    & wcc386.exe -q -wcd=303 -s -zls -zc -mf -dVXD32 -fpi87 -ei -oeatxhn -6s -fp6 "$projectRoot\src\vxd\g98cpu.c" '-fo=g98cpu.obj'
    if ($LASTEXITCODE) { throw 'CPU VxD compile failed' }
    @'
system win_vxd dynamic
option quiet
option map=g98cpu.map
option nodefaultlibs
name g98cpu.vxd
file g98cpu.obj
segment '_TEXT' PRELOAD NONDISCARDABLE IOPL
segment '_DATA' PRELOAD NONDISCARDABLE IOPL
segment 'CONST' PRELOAD NONDISCARDABLE IOPL
segment 'CONST2' PRELOAD NONDISCARDABLE IOPL
export VXD_DDB.1
'@ | Set-Content -Encoding ASCII g98cpu.lnk
    & wlink.exe '@g98cpu.lnk'
    if ($LASTEXITCODE) { throw 'CPU VxD link failed' }
    & .\fixlink.exe -vxd32 g98cpu.vxd
    if ($LASTEXITCODE) { throw 'CPU VxD header fix failed' }
    $path = Join-Path $output 'g98cpu.vxd'
    $bytes = [IO.File]::ReadAllBytes($path)
    $le = [BitConverter]::ToInt32($bytes, 0x3C)
    if ($bytes[$le] -ne 0x4C -or $bytes[$le + 1] -ne 0x45) { throw 'Expected an LE image' }
    # Enable the internal relocations emitted by wlink.
    $bytes[$le + 0x12] = $bytes[$le + 0x12] -band 0xFE
    # Ordinal 1 must resolve a 32-bit DDB, not a 286 call gate.
    $entry = $le + [BitConverter]::ToUInt32($bytes, $le + 0x5C)
    if ($bytes[$entry] -ne 1) { throw 'Expected a single DDB export' }
    if ($bytes[$entry + 1] -eq 2) {
        $bytes[$entry + 1] = 3
        $bytes[$entry + 4] = 1
    }
    if ($bytes[$entry + 1] -ne 3) { throw 'Expected a 32-bit DDB export' }
    # This driver exports no numeric services. Keep its DDB device ID unassigned
    # so another installed VxD cannot collide with an arbitrary OEM number.
    $ddbName = [Text.Encoding]::ASCII.GetBytes('G98CPU  ')
    $ddbMatches = 0
    for ($offset = 12; $offset -le $bytes.Length - $ddbName.Length; $offset++) {
        $match = $true
        for ($index = 0; $index -lt $ddbName.Length; $index++) {
            if ($bytes[$offset + $index] -ne $ddbName[$index]) { $match = $false; break }
        }
        if ($match) {
            $ddbMatches++
            # DDB_Req_Device_Number is six bytes before DDB_Name.
            if ([BitConverter]::ToUInt16($bytes, $offset - 6) -ne 0) {
                throw 'G98CPU must use UNDEFINED_DEVICE_ID'
            }
        }
    }
    if ($ddbMatches -ne 1) { throw 'Expected exactly one G98CPU DDB' }
    [IO.File]::WriteAllBytes($path, $bytes)
    & wcl386.exe -q -bt=nt -l=nt "$projectRoot\scripts\probe-vxd.c" '-fe=CPUCheck.exe' winmm.lib
    if ($LASTEXITCODE) { throw 'CPU test utility build failed' }
    & wcl386.exe -q -bt=nt -l=nt "$projectRoot\scripts\setup-vxd.c" '-fe=CPUSetup.exe'
    if ($LASTEXITCODE) { throw 'CPU driver setup build failed' }
    & wcl386.exe -q -bt=nt -l=nt -3r -os "$projectRoot\scripts\diagnose-cpu.c" '-fe=CPUDiag.exe' advapi32.lib
    if ($LASTEXITCODE) { throw 'CPU driver diagnostic build failed' }
    $package = Join-Path $projectRoot 'build\cpu-prototype'
    New-Item -ItemType Directory -Force -Path $package | Out-Null
    Copy-Item -LiteralPath $path, '.\CPUCheck.exe', '.\CPUSetup.exe', '.\CPUDiag.exe' -Destination $package
    Copy-Item -LiteralPath "$projectRoot\src\vxd\README.txt" -Destination $package
    Copy-Item -LiteralPath "$projectRoot\LICENSE" -Destination (Join-Path $package 'LICENSE.txt')
    Copy-Item -LiteralPath "$projectRoot\licenses\VMDISP9X.TXT" -Destination $package
    Copy-Item -LiteralPath "$projectRoot\NOTICE.txt", "$projectRoot\licenses\WATCOM.TXT" -Destination $package
    Write-Output "Built $path ($($bytes.Length) bytes)"
    Write-Output "Standalone test package: $package"
} finally { Pop-Location }
