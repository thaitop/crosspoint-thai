"""Tests for scripts/sim_check.py. Run: uvx pytest test/sim_check -v"""

import json
import re
import sys
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

import sim_check  # noqa: E402

SAMPLE_EPUB = REPO / "test" / "epubs" / "thai_sample.epub"


def test_seed_sd_is_fresh(tmp_path):
    dest = tmp_path / "sd"
    (dest / ".crosspoint").mkdir(parents=True)
    (dest / "stale.txt").write_text("old")
    (dest / ".crosspoint" / "state.json").write_text("{}")

    sim_check.seed_sd(dest, SAMPLE_EPUB)

    assert not (dest / "stale.txt").exists()
    assert not (dest / ".crosspoint" / "state.json").exists()
    assert (dest / "books" / "thai_sample.epub").is_file()
    settings = (dest / ".crosspoint" / "settings.json").read_text()
    assert json.loads(settings) == {"language": "TH"}


def test_sample_epub_valid():
    with zipfile.ZipFile(SAMPLE_EPUB) as epub:
        first = epub.infolist()[0]
        assert first.filename == "mimetype"
        assert first.compress_type == zipfile.ZIP_STORED
        assert epub.read("mimetype") == b"application/epub+zip"
        text = "".join(
            epub.read(n).decode("utf-8") for n in epub.namelist() if n.endswith(".xhtml")
        )
    assert "ที่" in text and "น้ำ" in text
    assert re.search(r"[฀-๿]{20,}", text)
    assert re.search(r"[0-9]", text)


def test_check_log_reports_missing():
    log = "[1] Entering activity: Home\n[2] Entering activity: EpubReader\n"
    assert sim_check.check_log(log, sim_check.EXPECTED_ACTIVITIES) == ["FileBrowser", "Settings"]
    full = "\n".join(f"[{i}] Entering activity: {n}" for i, n in
                     enumerate(["Boot", "Home", "FileBrowser", "EpubReader", "Home", "SettingsList"]))
    assert sim_check.check_log(full, sim_check.EXPECTED_ACTIVITIES) == []


def test_check_log_requires_order():
    log = "Entering activity: Settings\nEntering activity: Home\n"
    assert sim_check.check_log(log, ["Home", "Settings"]) == ["Settings"]


GOOD_LOG = "\n".join([
    "Loaded SD card font family: GoogleSans",
    *(f"Entering activity: {n}" for n in ["Home", "FileBrowser", "EpubReader", "SettingsList"]),
    "[SIM] SCREEN: 100% | การตั้งค่า",
])


def _fake_env(monkeypatch, build_ok=lambda env: True, launch=None):
    monkeypatch.setattr(sim_check, "FONT_DIR", Path("/fonts/GoogleSans"))
    monkeypatch.setattr(sim_check, "seed_sd", lambda dest, book, font_dir=None: dest.mkdir(parents=True))
    monkeypatch.setattr(sim_check, "build_env", build_ok)
    if launch is not None:
        monkeypatch.setattr(sim_check, "launch", launch)
    monkeypatch.setattr(sim_check, "to_png", lambda bmp: bmp)


def test_run_device_timeout_marks_failed(tmp_path, monkeypatch):
    real_launch = sim_check.launch

    def slow_launch(env, env_vars, log_path, timeout_s):
        return real_launch(env, env_vars, log_path, timeout_s, cmd=["sleep", "5"])

    _fake_env(monkeypatch, launch=slow_launch)
    result = sim_check.run_device("x4", tmp_path, timeout_s=1)
    assert result.ok is False
    assert "timeout" in result.reasons
    assert "missing screenshot: home" in result.reasons
    assert result.shots["home"] is None


def test_run_device_pass(tmp_path, monkeypatch):
    def good_launch(env, env_vars, log_path, timeout_s):
        log_path.write_text(GOOD_LOG)
        for step in sim_check.STEPS:
            (log_path.parent / f"{step}.bmp").write_bytes(f"BM{step}".encode())
        return 0

    _fake_env(monkeypatch, launch=good_launch)
    result = sim_check.run_device("x4", tmp_path)
    assert result.ok, result.reasons
    assert set(result.shots) == set(sim_check.STEPS)


def test_build_failure_continues(tmp_path, monkeypatch, capsys):
    def good_launch(env, env_vars, log_path, timeout_s):
        log_path.write_text(GOOD_LOG)
        for step in sim_check.STEPS:
            (log_path.parent / f"{step}.bmp").write_bytes(f"BM{step}".encode())
        return 0

    _fake_env(monkeypatch, build_ok=lambda env: env != "simulator_x3", launch=good_launch)
    monkeypatch.setattr(sim_check, "OUT_ROOT", tmp_path)
    font = tmp_path / "GoogleSans"
    font.mkdir()
    monkeypatch.setattr(sim_check, "FONT_DIR", font)
    assert sim_check.main(["x3", "x4"]) == 1
    out = capsys.readouterr().out
    assert "x3: FAIL" in out and "build failed" in out
    assert "x4: PASS" in out


def test_main_rejects_unknown_device(capsys):
    assert sim_check.main(["nope"]) == 2


def test_gallery_keeps_native_size(tmp_path):
    png = tmp_path / "home.png"
    png.write_bytes(b"x")
    ok = sim_check.DeviceResult("x3", True, [], {s: (png if s == "home" else None) for s in sim_check.STEPS}, "")
    bad = sim_check.DeviceResult("picco", False, ["timeout"], {s: None for s in sim_check.STEPS}, "LAST LOG LINE")
    html = sim_check.render_gallery([ok, bad], tmp_path).read_text()
    assert "Simulator check — not e-paper validation." in html
    assert not re.search(r"<img[^>]*\b(width|height)=", html)
    assert not re.search(r"img\s*{[^}]*\bwidth\s*:", html)
    assert "timeout" in html and "LAST LOG LINE" in html


def test_every_device_script_shoots_every_step():
    for name, device in sim_check.DEVICES.items():
        script = device.input_script.format(dir="/out")
        for step in sim_check.STEPS:
            assert f"SHOT:/out/{step}.bmp" in script, (name, step)
        assert script.endswith("QUIT"), name


def test_script_times_never_go_backwards_within_a_wait_segment():
    # The simulator runs items in time order from the last matched WAIT, so a
    # WAIT timed earlier than the key before it would match a stale log line.
    for name, device in sim_check.DEVICES.items():
        last = 0
        for item in device.input_script.split(";"):
            ms, action = item.split(":", 1)
            assert int(ms) >= last, (name, item)
            last = 0 if action.startswith("WAIT:") else int(ms)


def test_run_device_fails_when_page_does_not_turn(tmp_path, monkeypatch):
    def same_page_launch(env, env_vars, log_path, timeout_s):
        log_path.write_text(GOOD_LOG)
        for step in sim_check.STEPS:
            data = b"BMpage" if step.startswith("book_") else f"BM{step}".encode()
            (log_path.parent / f"{step}.bmp").write_bytes(data)
        return 0

    _fake_env(monkeypatch, launch=same_page_launch)
    result = sim_check.run_device("x4", tmp_path)
    assert result.ok is False
    assert "page turn not visible" in result.reasons


def test_seed_sd_installs_reader_font(tmp_path):
    font_dir = tmp_path / "GoogleSans"
    font_dir.mkdir()
    (font_dir / "GoogleSans_16.cpfont").write_bytes(b"font")
    dest = tmp_path / "sd"

    sim_check.seed_sd(dest, SAMPLE_EPUB, font_dir)

    assert (dest / ".fonts" / "GoogleSans" / "GoogleSans_16.cpfont").read_bytes() == b"font"
    settings = json.loads((dest / ".crosspoint" / "settings.json").read_text())
    assert settings == {"language": "TH", "sdFontFamilyName": "GoogleSans"}


def test_main_requires_font_dir(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(sim_check, "FONT_DIR", tmp_path / "missing")
    assert sim_check.main(["x4"]) == 2
    assert "SIM_CHECK_FONT_DIR" in capsys.readouterr().out


def test_presses_are_held_long_enough_to_survive_a_loop_stall():
    # A default 80 ms press can fall inside one slow firmware loop and be lost.
    for name, device in sim_check.DEVICES.items():
        for item in device.input_script.split(";"):
            action = item.split(":", 1)[1]
            if action.startswith(("WAIT:", "SHOT:", "SWIPE:")) or action == "QUIT":
                continue
            if action.startswith("TAP:"):
                parts = action[4:].split(",")
                assert len(parts) == 3 and int(parts[2]) >= 150, (name, item)
            else:
                key, _, hold = action.partition(":")
                assert hold and int(hold) >= 150, (name, item)


def _launch_with_log(log):
    def fake(env, env_vars, log_path, timeout_s):
        log_path.write_text(log)
        for step in sim_check.STEPS:
            (log_path.parent / f"{step}.bmp").write_bytes(f"BM{step}".encode())
        return 0
    return fake


def test_run_device_fails_when_reader_font_not_loaded(tmp_path, monkeypatch):
    _fake_env(monkeypatch, launch=_launch_with_log(GOOD_LOG.replace("Loaded SD card font family: GoogleSans", "")))
    result = sim_check.run_device("x4", tmp_path)
    assert "reader font not loaded" in result.reasons


def test_run_device_fails_without_thai_screen_text(tmp_path, monkeypatch):
    _fake_env(monkeypatch, launch=_launch_with_log(GOOD_LOG.replace("การตั้งค่า", "Settings")))
    result = sim_check.run_device("x4", tmp_path)
    assert "no Thai text on screen" in result.reasons


def test_main_runs_each_device_once(tmp_path, monkeypatch, capsys):
    _fake_env(monkeypatch, launch=_launch_with_log(GOOD_LOG))
    monkeypatch.setattr(sim_check, "OUT_ROOT", tmp_path)
    font = tmp_path / "GoogleSans"
    font.mkdir()
    monkeypatch.setattr(sim_check, "FONT_DIR", font)
    assert sim_check.main(["x4", "x4"]) == 0
    assert capsys.readouterr().out.count("x4: PASS") == 1


def test_main_requires_sample_book(tmp_path, monkeypatch, capsys):
    font = tmp_path / "GoogleSans"
    font.mkdir()
    monkeypatch.setattr(sim_check, "FONT_DIR", font)
    monkeypatch.setattr(sim_check, "SAMPLE_BOOK", tmp_path / "missing.epub")
    assert sim_check.main(["x4"]) == 2
    assert "generate_thai_sample_epub.py" in capsys.readouterr().out


def test_main_requires_pio(tmp_path, monkeypatch, capsys):
    font = tmp_path / "GoogleSans"
    font.mkdir()
    monkeypatch.setattr(sim_check, "FONT_DIR", font)
    monkeypatch.setattr(sim_check, "PIO", str(tmp_path / "no-pio"))
    assert sim_check.main(["x4"]) == 2
    assert "PlatformIO not found" in capsys.readouterr().out
