# Device Simulator Check Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One command builds all nine device profiles in the desktop simulator, walks the same Thai UI and Thai book path on each, and writes one HTML screenshot gallery with pass/fail per device.

**Architecture:** Simulator PlatformIO envs live in a new `platformio.simulator.ini`, which `platformio.ini` loads through `extra_configs`, so device envs stay untouched. A Python script (`scripts/sim_check.py`) seeds a fresh SD folder per device (Thai settings + a generated Thai EPUB), runs the simulator with `CROSSPOINT_SIM_INPUT_SCRIPT` / `CROSSPOINT_SIM_SCREENSHOTS`, checks the log for the expected activities, and renders `index.html`.

**Tech Stack:** PlatformIO native platform, `crosspoint-reader/crosspoint-simulator` (SDL2), Python 3.12 (stdlib only, run with `uv`), pytest via `uvx`, macOS `sips` for BMP→PNG.

**Spec:** `docs/superpowers/specs/2026-11-26-device-simulator-check-design.md`

> **As built:** interfaces changed during execution (script built by `make_script`, `seed_sd(dest, book, font_dir)`, WAIT timing, font check). The spec's "As built" note and `scripts/sim_check.py` are authoritative.

## Global Constraints

- pio binary: `~/.platformio/penv/bin/pio` (pioarduino 6.1.19). Not on `PATH`.
- `default_envs = default` must not change; no device env (`default`, `x4pro`, `x4c`, `sticky`, `papermono`, `metalio_eink4`, `eego_a4`, `picco`, `*-gh_release*`, `slim`) may change.
- Simulator revision: try the latest official commit first. If it does not build, pin `8dcf00b0` (2026-10-31). Never fork the simulator. Record the pinned SHA in `lib_deps` (`simulator=https://github.com/crosspoint-reader/crosspoint-simulator#<sha>`).
- `src/` changes: compile-only `#ifdef SIMULATOR` fixes. If more is needed, stop and report to the owner.
- Seeded settings: `/.crosspoint/settings.json` with `"language": "TH"` (code string, `src/CrossPointSettings.cpp:294-296`, `lib/I18n/I18nStrings.cpp:41`).
- Expected log line format: `Entering activity: <Name>` (`src/activities/Activity.cpp:5`). Names used: `Home`, `FileBrowser`, `EpubReader`, `Settings` (confirm `Settings` name with `grep -rn 'Activity("Settings' src`).
- Output dir: `build/sim-check/<YYYYmmdd-HHMMSS>/`. Gallery header text: "Simulator check — not e-paper validation."
- Sample book text: original Thai text written for this project, not copyrighted text.
- Commits need explicit owner approval (`CLAUDE.md`). Never push. Firmware-handoff runs before declaring done (Task 5).

## Review Focus

- Thai rendering on the 9 different screen sizes/orientations (X3 792x528, EEGO A4 768x552): the gallery must show each device's real frame size, not one scaled to fit the X4 size. Owned by Task 4 (`test_gallery_keeps_native_size`).
- The simulator crashes or hangs: a per-device timeout must kill the process and mark the device failed, not stall the whole run. Owned by Task 4 (`test_run_device_timeout_marks_failed`).
- Stale state between runs: the firmware reopens the last book. Each device must get a fresh SD folder. Owned by Task 3 (`test_seed_sd_is_fresh`).
- Build failure of one device must not stop the others. The summary still lists all nine. Owned by Task 4 (`test_build_failure_continues`).
- Activity line present but screenshot missing (or the reverse): each is its own failure reason in the gallery. Owned by Task 4 (`test_check_log_reports_missing`).

---

### Task 1: Base simulator env builds and runs on X4

Find a working simulator revision and the compile fixes this fork needs. This is the risky task. If the fixes go past compile-only, stop here and report.

**Files:**
- Create: `platformio.simulator.ini`
- Modify: `platformio.ini:4` (`extra_configs = platformio.local.ini, platformio.simulator.ini`; check PlatformIO accepts a comma/newline list)
- Possibly modify: files under `src/` that fail to compile (guarded compile fixes only)

**Interfaces:**
- Produces: env `simulator` (X4, base), runnable as `~/.platformio/penv/bin/pio run -e simulator -t run_simulator`; build output binary path (find it under `.pio/build/simulator/`, e.g. `program`). Task 4 needs the binary path and how `run_simulator` launches it (read the simulator's `run_simulator.py`).

- [ ] **Step 1:** Copy `[env:simulator]` from the simulator's `sample-platformio-macos.ini` (latest) into `platformio.simulator.ini`. Keep its `build_src_filter` exclusions and `lib_deps` SDK symlinks. Add this fork's extra `lib_deps` from `[base]` in `platformio.ini` that the sample lacks (diff the two lists).
- [ ] **Step 2:** Run `~/.platformio/penv/bin/pio run -e simulator 2>&1 | tail -40`. Expected: success, or a list of compile errors.
- [ ] **Step 3:** If the errors come from the simulator not matching this fork's firmware (missing HAL methods, upstream-only APIs), switch `lib_deps` to `#8dcf00b0`, remove `.pio/libdeps/simulator`, and rebuild. If the errors come from this fork's own code (Thai renderer, calendar, etc.), add minimal `#ifdef SIMULATOR` compile fixes.
- [ ] **Step 4:** Pin the SHA that works in `lib_deps`.
- [ ] **Step 5:** Run `~/.platformio/penv/bin/pio run -e simulator -t run_simulator`. Expected: an SDL window shows the Home screen. Close it.
- [ ] **Step 6:** Confirm device builds are unaffected: `git diff platformio.ini` shows only the `extra_configs` line.
- [ ] **Step 7:** Ask the owner to approve a commit of `platformio.simulator.ini`, `platformio.ini`, and any `src/` fixes.

### Task 2: All nine simulator envs build

**Files:**
- Modify: `platformio.simulator.ini`

**Interfaces:**
- Consumes: `[env:simulator]` from Task 1.
- Produces: envs `simulator`, `simulator_x3`, `simulator_x4_pro`, `simulator_x4_classic`, `simulator_sticky`, `simulator_papermono`, `simulator_metalio_eink4`, `simulator_eego_a4`, `simulator_picco`. Each one `extends = env:simulator` with the `SIMULATOR_DEVICE_*` flag and the `FREEINK_DEVICE_*` define from the spec table. Copy the flag blocks (including `-UFREEINK_DEVICE_X4` and `-DBOARD_HAS_PSRAM` where the sample uses them) from the pinned simulator's sample ini.

- [ ] **Step 1:** Add the eight device envs.
- [ ] **Step 2:** Build each: `for e in simulator_x3 simulator_x4_pro simulator_x4_classic simulator_sticky simulator_papermono simulator_metalio_eink4 simulator_eego_a4 simulator_picco; do ~/.platformio/penv/bin/pio run -e $e >/tmp/$e.log 2>&1 && echo "$e OK" || echo "$e FAIL"; done`. Expected: 8 × OK. Fix failures under the same rules as Task 1 Step 3.
- [ ] **Step 3:** Ask the owner to approve a commit.

### Task 3: Thai sample book and fresh SD seeding

**Files:**
- Create: `scripts/generate_thai_sample_epub.py` (follows `scripts/generate_supsub_test_epub.py`; writes `test/epubs/thai_sample.epub`)
- Create: `test/epubs/thai_sample.epub` (generated, committed like the other test EPUBs)
- Create: `scripts/sim_check.py` (seeding part only in this task)
- Test: `test/sim_check/test_sim_check.py`

**Interfaces:**
- Produces: `seed_sd(dest: Path, book: Path) -> None` in `scripts/sim_check.py`. It deletes `dest` if present, then creates `dest/books/<book.name>` and `dest/.crosspoint/settings.json` = `{"language": "TH"}`.

- [ ] **Step 1: Write failing tests** in `test/sim_check/test_sim_check.py` (import `scripts/sim_check.py` via `sys.path`):
  - `test_seed_sd_is_fresh`: create `dest/stale.txt` and `dest/.crosspoint/state.json`, call `seed_sd`. Assert both are gone, `dest/books/thai_sample.epub` exists, and `json.loads(settings) == {"language": "TH"}`.
  - `test_sample_epub_valid`: open `test/epubs/thai_sample.epub` as a zip. Assert the first entry is `mimetype`, stored uncompressed, containing `application/epub+zip`. The chapter XHTML contains a combining-mark cluster (e.g. `ที่`, `น้ำ`), a run of Thai letters of 20 or more characters with no space, and an ASCII digit.
- [ ] **Step 2:** Run `uvx pytest test/sim_check -v`. Expected: FAIL (missing module / file).
- [ ] **Step 3:** Write the generator. It produces 2 chapters of original Thai text, long enough for at least 3 pages on X4 at default font size, mixed with English words and numbers. Run `uv run -p 3.12 scripts/generate_thai_sample_epub.py`. Implement `seed_sd`.
- [ ] **Step 4:** Run `uvx pytest test/sim_check -v`. Expected: PASS.
- [ ] **Step 5:** Manual check: `seed_sd(Path("fs_"), ...)`, then `pio run -e simulator -t run_simulator`. The Home screen is in Thai, and the book opens with readable Thai. If a partial `settings.json` is rejected (English UI), extend the seed with the keys `CrossPointSettings::loadFromFile` requires and update the test.
- [ ] **Step 6:** Ask the owner to approve a commit.

### Task 4: Scripted run, log check, gallery

**Files:**
- Modify: `scripts/sim_check.py`
- Test: `test/sim_check/test_sim_check.py`

**Interfaces:**
- Consumes: `seed_sd` (Task 3), env names (Task 2), binary path and launch method (Task 1).
- Produces (all in `scripts/sim_check.py`):
  - `DEVICES: dict[str, DeviceSteps]`, keyed by short name (`x4`, `x3`, `x4_pro`, `x4_classic`, `sticky`, `papermono`, `metalio_eink4`, `eego_a4`, `picco`). `DeviceSteps` holds `env: str`, `input_script: str`, `screenshots: list[tuple[int, str]]` (ms, step name). Step names: `home`, `files`, `book_p1`, `book_p2`, `settings`.
  - `EXPECTED_ACTIVITIES = ["Home", "FileBrowser", "EpubReader", "Settings"]`
  - `check_log(log: str, expected: list[str]) -> list[str]`: returns the missing names, checked in order.
  - `run_device(name: str, out: Path, timeout_s: int = 90) -> DeviceResult`. Builds (`pio run -e <env>`), seeds `out/<name>/sd`, runs the binary with `CROSSPOINT_SIM_SD`, `CROSSPOINT_SIM_INPUT_SCRIPT`, `CROSSPOINT_SIM_SCREENSHOTS` (paths under `out/<name>/`), captures the log to `out/<name>/log.txt`, and converts BMPs to PNG with `sips`. `DeviceResult` holds `name`, `ok: bool`, `reasons: list[str]`, `shots: dict[str, Path | None]`, `log_tail: str` (last 30 lines).
  - `render_gallery(results: list[DeviceResult], out: Path) -> Path`: writes `out/index.html` and returns its path.
  - `main(argv) -> int`: optional device-name arguments (default: all). Prints `<name>: PASS|FAIL <reasons>` per device and the `index.html` path. Returns 1 if any device failed.
- Timing: on `8dcf00b0`, use fixed delays with at least 1500 ms between steps and the screenshot about 800 ms after each key. If the pinned simulator supports `WAIT:`/`SHOT:`, use `0:WAIT:Entering activity: <Name>` before each step instead (read from the README at the pinned SHA). Input per device: buttons via key names; for EEGO A4, Picco and Metalio, use the keys their simulator docs map (the `docs/*.md` at the pinned SHA), or `TAP:x,y` from layout coordinates.

- [ ] **Step 1: Write failing tests:**
  - `test_check_log_reports_missing`: log with `Entering activity: Home` and `Entering activity: EpubReader` → `["FileBrowser", "Settings"]`. A log with all four in order → `[]`.
  - `test_run_device_timeout_marks_failed`: monkeypatch the build and launch functions so the launch runs `sleep 5` with `timeout_s=1`. Assert `ok is False` and `"timeout"` is in `reasons`.
  - `test_build_failure_continues`: monkeypatch the build to fail for `x3` only and the runner to succeed for the rest. Assert `main(["x3", "x4"])` returns 1, and the printed summary has both `x3: FAIL` and `x4: PASS`.
  - `test_gallery_keeps_native_size`: render with one result whose PNG is 792x528. Assert the HTML `<img>` has no `width`/`height` attributes that force another size, and the CSS has no fixed width on images (use `max-width: 100%` or nothing). Assert the header text from Global Constraints is present, and a failed result renders its `reasons` and `log_tail`.
  - Each missing screenshot is listed in `reasons` as `missing screenshot: <step>`. Cover this inside `test_run_device_timeout_marks_failed` or a separate small test.
- [ ] **Step 2:** `uvx pytest test/sim_check -v`. Expected: the new tests FAIL.
- [ ] **Step 3:** Implement. Build and launch are separate small functions (`build_env(env) -> bool`, `launch(env, env_vars, log_path, timeout_s) -> int | None`) so the tests can monkeypatch them. Use `subprocess.run(..., timeout=)`; on `TimeoutExpired`, kill the process and record `"timeout"`.
- [ ] **Step 4:** `uvx pytest test/sim_check -v`. Expected: PASS.
- [ ] **Step 5:** Real run on one device: `uv run -p 3.12 scripts/sim_check.py x4_pro`. Open `index.html`. Expected: 5 screenshots in Thai that match their step names. Tune the timings if any shot is mid-transition.
- [ ] **Step 6:** Ask the owner to approve a commit.

### Task 5: Full run and handoff

- [ ] **Step 1:** `uv run -p 3.12 scripts/sim_check.py`. Expected: 9 × PASS. Open the gallery and check each device's shots show the right screen and readable Thai. Tune per-device inputs until they do.
- [ ] **Step 2:** Failure check: temporarily rename `test/epubs/thai_sample.epub` and run `scripts/sim_check.py x4`. Expected: FAIL with `EpubReader` missing, exit code 1. Restore the file.
- [ ] **Step 3:** Device builds still work: `~/.platformio/penv/bin/pio run -e x4pro` and `-e default`. Expected: success.
- [ ] **Step 4:** Add a short "Simulator check" section to `docs/contributing/` (or the existing testing doc): prerequisites (`brew install sdl2`), the command, where output goes, and that it does not replace hardware testing.
- [ ] **Step 5:** Run the `firmware-handoff` skill (`.agents/skills/firmware-handoff/SKILL.md`). The `platformio.ini` and any `src/` changes affect the firmware build.
- [ ] **Step 6:** Report the results to the owner and ask for commit approval. Do not push.
