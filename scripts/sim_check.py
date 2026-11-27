#!/usr/bin/env python3
"""Run every device profile in the desktop simulator and build a screenshot gallery.

Usage: uv run -p 3.12 scripts/sim_check.py [device ...]

Each device gets a fresh SD folder (Thai UI, test/epubs/thai_sample.epub, reader font),
then the same path: Home -> file browser -> book page 1 -> page 2 -> Home again -> Settings.
Set SIM_CHECK_UI_THEME to a theme number (8 = Shelf) to seed that home theme.
Output: build/sim-check/<timestamp>/index.html. Exit code 1 if any device failed.
This is a simulator check, not e-paper validation.
"""

import html
import json
import re
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT_ROOT = REPO / "build" / "sim-check"
SAMPLE_BOOK = REPO / "test" / "epubs" / "thai_sample.epub"
# Reader font copied to every simulated SD card. Not in the repo: set
# SIM_CHECK_FONT_DIR to a folder, named after the font family, of .cpfont files.
FONT_DIR = Path(os.environ.get("SIM_CHECK_FONT_DIR", Path.home() / "Downloads" / "E-Reader" / "Fonts" / "GoogleSans"))
PIO = os.environ.get("PIO", str(Path.home() / ".platformio" / "penv" / "bin" / "pio"))

STEPS = ["home", "files", "book_p1", "book_p2", "home_book", "settings"]
# Log lines from src/activities/Activity.cpp and EpubReaderActivity.cpp.
EXPECTED_ACTIVITIES = ["Home", "FileBrowser", "EpubReader", "Settings"]
HEADER = "Simulator check — not e-paper validation."

PAGE_DRAWN = "WAIT:Rendered page in"
# Default presses last 80 ms and can fall inside one slow firmware loop.
HOLD_MS = 200


def held(action: str) -> str:
    """Add HOLD_MS to a key press or tap; leave swipes as they are."""
    if action.startswith("TAP:"):
        return f"{action},{HOLD_MS}"
    if action.startswith("SWIPE:"):
        return action
    return f"{action}:{HOLD_MS}"


def make_script(open_files: str, open_item: str, next_page: str, go_home: str,
                open_settings: list[str]) -> str:
    """Build a simulator input script (crosspoint-simulator README, "Automated QA").

    Actions are simulator script actions such as ENTER or TAP:0.3,0.18. Times
    after a WAIT count from that WAIT. {dir} is filled with the output folder.
    """
    open_files, open_item, next_page, go_home = map(held, (open_files, open_item, next_page, go_home))
    # Home loads the recent book's covers after its first paint; shoot it once
    # they are drawn, then navigate.
    settings = [f"{3000 + 500 * i}:{held(a)}" for i, a in enumerate(open_settings)]
    items = [
        "WAIT:Entering activity: Home", "800:SHOT:{dir}/home.bmp",
        f"1000:{open_files}", "WAIT:Entering activity: FileBrowser", "800:SHOT:{dir}/files.bmp",
        f"1000:{open_item}", "WAIT:SCREEN: .*thai_sample", f"500:{open_item}",
        "WAIT:Entering activity: EpubReader", PAGE_DRAWN, "800:SHOT:{dir}/book_p1.bmp",
        f"1000:{next_page}", PAGE_DRAWN, "800:SHOT:{dir}/book_p2.bmp",
        f"1000:{go_home}", "WAIT:Entering activity: Home", "2500:SHOT:{dir}/home_book.bmp", *settings,
        "WAIT:Entering activity: Settings", "800:SHOT:{dir}/settings.bmp", "1200:QUIT",
    ]
    # Items run in time order from the last matched WAIT, so a WAIT must be
    # timed just after the action before it or it matches a stale log line.
    out, last = [], 0
    for item in items:
        if item.startswith("WAIT:"):
            out.append(f"{last + 1 if last else 0}:{item}")
            last = 0
        else:
            out.append(item)
            last = int(item.split(":", 1)[0])
    return ";".join(out)


# Front buttons: Confirm opens, Right turns the page, Back returns, Up wraps to Settings.
BUTTON_SCRIPT = make_script("ENTER", "ENTER", "RIGHT", "BACK", ["UP", "ENTER"])
# Metalio has no Back/Left/Right: cover Next turns the page, cover Home returns.
METALIO_SCRIPT = make_script("ENTER", "ENTER", "NEXT", "HOME", ["UP", "ENTER"])
# Picco has no Back or Home key: side Down turns the page, a swipe up returns Home.
PICCO_SCRIPT = make_script("ENTER", "ENTER", "DOWN", "SWIPE:0.5,0.98,0.5,0.5", ["UP", "ENTER"])
# EEGO A4 has only Up/Down/Power: touch for lists, the screen key for Home.
# Taps: home "Files" tool, first file-list row, home "Settings" tool. The
# Calendar (default, 7) and Shelf (8) homes share the capsule tool bar; keep
# TOOL_Y in step with CAPSULE_BAR_HEIGHT in src/components/HomeWidgets.cpp.
CAPSULE_BAR = os.environ.get("SIM_CHECK_UI_THEME", "7") in ("7", "8")
TOOL_Y = 0.947 if CAPSULE_BAR else 0.9
EEGO_SCRIPT = make_script(f"TAP:0.12,{TOOL_Y}", "TAP:0.3,0.18", "DOWN", "HOME", [f"TAP:0.87,{TOOL_Y}"])


@dataclass
class DeviceSteps:
    env: str
    input_script: str


@dataclass
class DeviceResult:
    name: str
    ok: bool
    reasons: list[str]
    shots: dict[str, Path | None]
    log_tail: str


DEVICES: dict[str, DeviceSteps] = {
    "x4": DeviceSteps("simulator", BUTTON_SCRIPT),
    "x3": DeviceSteps("simulator_x3", BUTTON_SCRIPT),
    "x4_pro": DeviceSteps("simulator_x4_pro", BUTTON_SCRIPT),
    "x4_classic": DeviceSteps("simulator_x4_classic", BUTTON_SCRIPT),
    "sticky": DeviceSteps("simulator_sticky", BUTTON_SCRIPT),
    "papermono": DeviceSteps("simulator_papermono", BUTTON_SCRIPT),
    "metalio_eink4": DeviceSteps("simulator_metalio_eink4", METALIO_SCRIPT),
    "eego_a4": DeviceSteps("simulator_eego_a4", EEGO_SCRIPT),
    "picco": DeviceSteps("simulator_picco", PICCO_SCRIPT),
}


def seed_sd(dest: Path, book: Path, font_dir: Path | None = None) -> None:
    """Make a fresh simulator SD folder: Thai UI, one sample book, optional reader font."""
    if dest.exists():
        shutil.rmtree(dest)
    (dest / "books").mkdir(parents=True)
    shutil.copy2(book, dest / "books" / book.name)
    settings = {"language": "TH"}
    if "SIM_CHECK_UI_THEME" in os.environ:
        settings["uiTheme"] = int(os.environ["SIM_CHECK_UI_THEME"])
    if font_dir is not None:
        shutil.copytree(font_dir, dest / ".fonts" / font_dir.name)
        settings["sdFontFamilyName"] = font_dir.name
    (dest / ".crosspoint").mkdir()
    (dest / ".crosspoint" / "settings.json").write_text(json.dumps(settings))


def check_log(log: str, expected: list[str]) -> list[str]:
    """Return expected activity names not entered, in order (prefix match)."""
    entered = [line.split("Entering activity: ", 1)[1].strip()
               for line in log.splitlines() if "Entering activity: " in line]
    missing, pos = [], 0
    for name in expected:
        hit = next((i for i in range(pos, len(entered)) if entered[i].startswith(name)), None)
        if hit is None:
            missing.append(name)
        else:
            pos = hit + 1
    return missing


def build_env(env: str) -> bool:
    proc = subprocess.run([PIO, "run", "-e", env], cwd=REPO, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout[-3000:] + proc.stderr[-3000:])
    return proc.returncode == 0


def launch(env: str, env_vars: dict[str, str], log_path: Path, timeout_s: int,
           cmd: list[str] | None = None) -> int | None:
    """Run the simulator binary; return its exit code, or None on timeout."""
    cmd = cmd or [str(REPO / ".pio" / "build" / env / "program")]
    with log_path.open("w") as log:
        try:
            return subprocess.run(cmd, cwd=REPO, env={**os.environ, **env_vars}, stdout=log,
                                  stderr=subprocess.STDOUT, timeout=timeout_s).returncode
        except subprocess.TimeoutExpired:
            return None


def to_png(bmp: Path) -> Path:
    png = bmp.with_suffix(".png")
    proc = subprocess.run(["sips", "-s", "format", "png", str(bmp), "--out", str(png)],
                          capture_output=True)
    return png if proc.returncode == 0 else bmp


def run_device(name: str, out: Path, timeout_s: int = 120) -> DeviceResult:
    device = DEVICES[name]
    ddir = out / name
    ddir.mkdir(parents=True, exist_ok=True)
    no_shots = {s: None for s in STEPS}
    if not build_env(device.env):
        return DeviceResult(name, False, ["build failed"], no_shots,
                            f"pio run -e {device.env} failed")
    seed_sd(ddir / "sd", SAMPLE_BOOK, FONT_DIR)
    log_path = ddir / "log.txt"
    rc = launch(device.env, {
        "CROSSPOINT_SIM_SD": str(ddir / "sd"),
        "CROSSPOINT_SIM_INPUT_SCRIPT": device.input_script.format(dir=ddir),
        "CROSSPOINT_SIM_SCREEN_TEXT": "1",
        "CROSSPOINT_SIM_WAIT_TIMEOUT_MS": "20000",
    }, log_path, timeout_s)
    reasons = []
    if rc is None:
        reasons.append("timeout")
    elif rc != 0:
        reasons.append(f"exit code {rc}")
    log = log_path.read_text(errors="replace") if log_path.exists() else ""
    reasons += [f"missing activity: {n}" for n in check_log(log, EXPECTED_ACTIVITIES)]
    # The firmware silently falls back to its built-in font or English UI.
    if FONT_DIR is not None and f"Loaded SD card font family: {FONT_DIR.name}" not in log:
        reasons.append("reader font not loaded")
    if not re.search(r"\[SIM\] SCREEN:.*[\u0E00-\u0E7F]", log):
        reasons.append("no Thai text on screen")
    shots: dict[str, Path | None] = {}
    for step in STEPS:
        bmp = ddir / f"{step}.bmp"
        shots[step] = to_png(bmp) if bmp.exists() else None
        if shots[step] is None:
            reasons.append(f"missing screenshot: {step}")
    p1, p2 = ddir / "book_p1.bmp", ddir / "book_p2.bmp"
    if p1.exists() and p2.exists() and p1.read_bytes() == p2.read_bytes():
        reasons.append("page turn not visible")
    tail = "\n".join(log.splitlines()[-30:])
    return DeviceResult(name, not reasons, reasons, shots, tail)


def render_gallery(results: list[DeviceResult], out: Path) -> Path:
    head = "".join(f"<th>{s}</th>" for s in STEPS)
    rows = []
    for r in results:
        status = "PASS" if r.ok else "FAIL"
        cells = []
        for step in STEPS:
            shot = r.shots.get(step)
            if shot is None:
                cells.append('<td class="miss">missing</td>')
            else:
                rel = html.escape(os.path.relpath(shot, out))
                cells.append(f'<td><a href="{rel}"><img src="{rel}" alt="{r.name} {step}"></a></td>')
        detail = ""
        if not r.ok:
            detail = (f'<tr class="fail"><td colspan="{len(STEPS) + 1}"><b>{html.escape(", ".join(r.reasons))}</b>'
                      f"<pre>{html.escape(r.log_tail)}</pre></td></tr>")
        rows.append(f'<tr class="{status.lower()}"><th>{r.name}<br>{status}</th>{"".join(cells)}</tr>{detail}')
    page = f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Simulator check</title>
<style>
body {{ font-family: sans-serif; margin: 16px; }}
table {{ border-collapse: collapse; }}
td, th {{ border: 1px solid #ccc; padding: 4px; vertical-align: top; }}
img {{ height: 360px; }}
tr.fail th, td.miss, tr.fail td {{ background: #fdd; }}
tr.pass th {{ background: #dfd; }}
pre {{ font-size: 11px; white-space: pre-wrap; }}
</style></head><body>
<h1>{HEADER}</h1>
<p>Images keep each device's own shape; click one for full size.</p>
<table><tr><th>device</th>{head}</tr>
{"".join(rows)}
</table></body></html>
"""
    index = out / "index.html"
    index.write_text(page, encoding="utf-8")
    return index


def main(argv: list[str]) -> int:
    names = list(dict.fromkeys(argv)) or list(DEVICES)
    unknown = [n for n in names if n not in DEVICES]
    if unknown:
        print(f"Unknown device(s): {', '.join(unknown)}. Known: {', '.join(DEVICES)}")
        return 2
    if not FONT_DIR.is_dir():
        print(f"Reader font folder not found: {FONT_DIR}. Set SIM_CHECK_FONT_DIR.")
        return 2
    if not SAMPLE_BOOK.is_file():
        print(f"Sample book not found: {SAMPLE_BOOK}. Run scripts/generate_thai_sample_epub.py.")
        return 2
    if not Path(PIO).is_file():
        print(f"PlatformIO not found: {PIO}. Set PIO to the pio binary.")
        return 2
    out = OUT_ROOT / time.strftime("%Y%m%d-%H%M%S")
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for name in names:
        result = run_device(name, out)
        results.append(result)
        print(f"{name}: {'PASS' if result.ok else 'FAIL ' + '; '.join(result.reasons)}", flush=True)
    print(f"Gallery: {render_gallery(results, out)}")
    return 0 if all(r.ok for r in results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
