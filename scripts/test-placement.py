"""Native compare-and-set tests against an isolated GLASSPRF install folder."""
from pathlib import Path
import configparser
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="w98-place-", dir=root / "build") as name:
    work = Path(name)
    exe = work / "GLASSPRF.EXE"
    shutil.copy2(root / "build/glassprf.exe", exe)
    base = ["3", "280", "0", "1", "3"]
    for i in range(28):
        base += ["1" if i in (0, 3) else "0", "32000" if i == 3 else "998",
                 "32000" if i == 3 else "20", "010203", "F0F1F2", "AABBCC", "64", "2"]

    def write(values):
        (work / "WIDGETS.INI").write_text("[Desktop]\nLayout=" + "|".join(values) + "\n")
        (work / "ACK.JS").write_text("untouched")

    def invoke(command):
        subprocess.run([str(exe), "w98widgetsglass:" + command], check=True, timeout=10)
        config = configparser.ConfigParser()
        config.read(work / "WIDGETS.INI")
        assert (work / "ACK.JS").read_text() == "untouched"
        return config["Desktop"]["Layout"].split("|"), config

    write(base)
    placed, _ = invoke("autoplace/3/998/307/280/0")
    expected = base.copy()
    expected[30:32] = ["998", "307"]
    assert placed == expected, "Only the requested coordinates may change"
    late, _ = invoke("autoplace/3/1/1/280/0")
    assert late == expected, "Late placement must not overwrite a saved/manual position"
    for edit, value in ((29, "0"), (1, "300"), (30, "500")):
        changed = base.copy()
        changed[edit] = value
        write(changed)
        skipped, _ = invoke("autoplace/3/998/307/280/0")
        assert skipped == changed, "Disabled/moved widget or changed width must be preserved"
    write(base)
    skipped, _ = invoke("autoplace/3/1/1/280/0/999x999")
    assert skipped == base, "Stale resolution placement must not edit the current layout"
    write(base)
    crowded, config = invoke("autoplace/3/998/20/280/1")
    assert config["Desktop"]["PlacementNotice"].startswith("No free space")
    assert crowded[29] == "0" and config["Desktop"]["LayoutHidden"][3] == "1"
    assert crowded[30:32] == ["998", "20"]
print("PASS: native placement preserves unrelated fields, stale edits and manager ACK")
