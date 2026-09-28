$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& (Join-Path $PSScriptRoot 'build.ps1')
$output=Join-Path $projectRoot 'build'
Push-Location $output
try {
    & wcl386.exe -q -bt=nt -l=nt_win -3r -os -dW98_SUITE '-fe=w98data.exe' "$projectRoot\src\bridge\datawriter.c" "$projectRoot\src\bridge\telemetry.c" "$projectRoot\src\bridge\extras.c" "$projectRoot\src\bridge\addons.c" advapi32.lib winmm.lib gdi32.lib shell32.lib ole32.lib
    if($LASTEXITCODE){throw 'suite telemetry build failed'}
    & wcl386.exe -q -bt=nt -l=nt -3r -os '-fe=glassctl.exe' "$projectRoot\src\glassctl\glassctl.c" ole32.lib advapi32.lib gdi32.lib
    if($LASTEXITCODE){throw 'glassctl build failed'}
    & wcl386.exe -q -bt=nt -l=nt_win -3r -os '-fe=glassprf.exe' "$projectRoot\src\glassctl\glasspref.c" "$projectRoot\src\glassctl\rss.c" "$projectRoot\src\glassctl\actions.c" "$projectRoot\src\glassctl\themes.c" "$projectRoot\src\glassctl\palette.c" "$projectRoot\src\glassctl\wallimage.c" oleaut32.lib gdi32.lib advapi32.lib comdlg32.lib shell32.lib ole32.lib wininet.lib winmm.lib wsock32.lib
    if($LASTEXITCODE){throw 'glass settings build failed'}
    & wcl386.exe -q -bt=nt -l=nt -3r -os '-fe=g98setup.exe' "$projectRoot\src\glassctl\setup.c"
    if($LASTEXITCODE){throw 'Glass98 installer build failed'}
    $env:INCLUDE="$env:WATCOM\h;$env:WATCOM\h\win"
    & wcl.exe -q -bt=windows -l=windows -ml -os '-fe=rsrc16.exe' "$projectRoot\src\bridge\rsrc16.c"
    if($LASTEXITCODE){throw 'Win16 resource helper build failed'}
    $env:INCLUDE="$env:WATCOM\h;$env:WATCOM\h\nt"
    # License/source notices are embedded in both PE and Win16 NE executables.
    & wrc.exe -q -r -bt=nt '-fo=notices32.res' "$projectRoot\src\notices.rc"
    if($LASTEXITCODE){throw '32-bit license resource build failed'}
    foreach($binary in @('gadgetctl.exe','w98data.exe','glassctl.exe','glassprf.exe','g98setup.exe')){
        & wrc.exe -q notices32.res $binary
        if($LASTEXITCODE){throw "License resource binding failed: $binary"}
    }
    & wrc.exe -q -r -bt=windows '-fo=notices16.res' "$projectRoot\src\notices.rc"
    if($LASTEXITCODE){throw '16-bit license resource build failed'}
    & wrc.exe -q -30 notices16.res rsrc16.exe
    if($LASTEXITCODE){throw 'Win16 license resource binding failed'}
} finally {Pop-Location}
$package=Join-Path $projectRoot 'dist\Glass98'
New-Item -ItemType Directory -Force -Path $package | Out-Null
# Remove an unsupported feed when reusing the package directory.
$obsoleteVu=Join-Path $package 'VU.JS'
if(Test-Path -LiteralPath $obsoleteVu){Remove-Item -LiteralPath $obsoleteVu}
foreach($name in @('gadgetctl.exe','w98data.exe','glassctl.exe','glassprf.exe','rsrc16.exe','g98setup.exe')){
    Copy-Item -LiteralPath (Join-Path $output $name) -Destination (Join-Path $package $name.ToUpperInvariant()) -Force
}
Copy-Item "$projectRoot\gadgets\glass\GLASS.HTM","$projectRoot\gadgets\glass\WALL.BMP","$projectRoot\gadgets\glass\WIDGETS.JS","$projectRoot\gadgets\glass\ADDONS.JS","$projectRoot\gadgets\glass\THEMES.JS","$projectRoot\gadgets\glass\MANAGER.JS","$projectRoot\gadgets\glass\MANAGER.CSS","$projectRoot\gadgets\glass\PLACEMENT.JS" $package -Force
$releaseVersion=([IO.File]::ReadAllText((Join-Path $projectRoot 'VERSION'))).Trim()
if($releaseVersion -notmatch '^A[0-9]{2}$'){throw 'Invalid stack version'}
$page=[IO.File]::ReadAllText((Join-Path $package 'GLASS.HTM')).Replace('@@VERSION@@',$releaseVersion)
[IO.File]::WriteAllText((Join-Path $package 'GLASS.HTM'),$page,[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'VERSION.TXT'),$releaseVersion+"`r`n",[Text.Encoding]::ASCII)
$settingsPattern = "var\s+settingsView\s*=\s*location\.search\s*==\s*'\?settings';"
$settingsPage = [IO.File]::ReadAllText((Join-Path $package 'GLASS.HTM'))
if ([regex]::Matches($settingsPage, $settingsPattern).Count -ne 1) {
    throw 'Expected exactly one settings-view declaration'
}
$settings = $settingsPage -replace $settingsPattern, 'var settingsView = true;'
[IO.File]::WriteAllText((Join-Path $package 'SETTINGS.HTM'),$settings,[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'DATA.JS'),'var snapshot=null;var sampleTime=0;',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'EXTRA.JS'),'var extraTime=0;',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'RSS.JS'),"var rssItems=[];var rssStatus='No feed configured';var rssTime=0;",[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'PING.JS'),'var pingResult={};',[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $package 'ACK.JS'),"var actionAck='';var ackTime=0;var ackOK=0;",[Text.Encoding]::ASCII)
foreach($name in @('INSTALL.BAT','REMOVE.BAT','README.TXT')){
    $content=[IO.File]::ReadAllText((Join-Path $projectRoot "packaging\glass\$name")) -replace "\r?\n","`r`n"
    [IO.File]::WriteAllText((Join-Path $package $name),$content,[Text.Encoding]::ASCII)
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE') -Destination (Join-Path $package 'LICENSE.TXT') -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'NOTICE.TXT') -Destination $package -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'licenses\WATCOM.TXT') -Destination $package -Force
Compress-Archive -Path (Join-Path $package '*') -DestinationPath (Join-Path $projectRoot 'dist\Glass98.zip') -Force
Write-Output "Glass98 package: $package"
