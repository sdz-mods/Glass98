"""Installer preference preparation tests against an isolated native helper."""
from pathlib import Path
import configparser
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="glass98-setup-", dir=root / "build") as folder:
    work = Path(folder)
    exe = work / "GLASSPRF.EXE"
    shutil.copy2(root / "build/glassprf.exe", exe)
    ini = work / "WIDGETS.INI"

    def prepare():
        subprocess.run([str(exe), "prepare"], check=True, timeout=10)
        result = configparser.ConfigParser()
        result.read(ini)
        return result

    ini.write_text("[Desktop]\nInitialWallpaperMode=2\nWallpaper=WALL.BMP\nLayout=invalid\n[Options]\nnotes=keep me\n")
    result = prepare()
    layout = result["Desktop"]["Layout"].split("|")
    assert len(layout) == 229 and layout[0] == "3" and layout[4] == "2"
    assert result["Options"]["notes"] == "keep me"
    before = ini.read_bytes()
    prepare()
    assert ini.read_bytes() == before, "valid settings should remain byte-identical"

    a00 = layout[:181]
    a00[8] = "123456"
    ini.write_text("[Desktop]\nLayout=" + "|".join(a00) + "\n[Options]\nnotes=keep A00 settings\n")
    expanded = prepare()["Desktop"]["Layout"].split("|")
    assert len(expanded) == 229
    assert expanded[8] == expanded[184] == "123456", "new widgets inherit the existing appearance"
    assert all(expanded[5 + i * 8] == "0" for i in range(22, 28))

    old = layout[:109]
    old[0] = "2"
    old[4] = "1"
    ini.write_text("[Desktop]\nLayout=" + "|".join(old) + "\n[Options]\nnotes=keep older settings\n")
    result = prepare()
    migrated = result["Desktop"]["Layout"].split("|")
    assert len(migrated) == 229 and migrated[0] == "3" and migrated[4] == "1"
    assert result["Options"]["notes"] == "keep older settings"
    assert "'disableAlpha':'0'" in (work / "PREFS.JS").read_text()
    # Verify opaque rendering persists through the native command/publish path.
    subprocess.run([str(exe), "w98widgetsglass:options/disableAlpha=31"], check=True, timeout=10)
    assert "'disableAlpha':'1'" in (work / "PREFS.JS").read_text()
    assert prepare()["Options"]["disablealpha"] == "1"
    subprocess.run([str(exe), "w98widgetsglass:options/disableAlpha=30"], check=True, timeout=10)
    assert "'disableAlpha':'0'" in (work / "PREFS.JS").read_text()
print("PASS: fresh/default wallpaper mode, invalid layout recovery, valid settings, old layout migration")
