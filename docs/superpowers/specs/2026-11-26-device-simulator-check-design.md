# Device simulator check — design

Date: 2026-11-26
Branch: `thai-sim-check`, from `thai-2.0.0` (Thai fork of CrossPoint Reader)

> **As built (2026-11-26):** implementation changed some details below. The
> entry point is `uv run -p 3.12 scripts/sim_check.py`. The pinned simulator
> `74a116dd` builds unchanged and has `WAIT:`/`SHOT:`, so no fallback was needed.
> The owner added the GoogleSans reader font (outside the repo, default
> `~/Downloads/E-Reader/Fonts/GoogleSans`, override `SIM_CHECK_FONT_DIR`). Each
> run also fails when page 2 equals page 1, the font is not loaded, or no Thai
> text is drawn. Gallery images use a fixed height and keep each device's shape.
> The usage guide is in `docs/contributing/testing-debugging.md`.

## Intent

The fork owner has only some of the devices this fork ships firmware for. No
specific bug has been reported; the problem is confidence. Before each `th-*`
release they want a quick, repeatable way to see that every device build still
boots and shows the Thai UI and Thai book text correctly.

Success: one command on the owner's Mac builds every supported device in the
desktop simulator, walks the same short path on each, and produces one HTML page
of screenshots that can be reviewed in a few minutes.

## Goals

- Add simulator PlatformIO environments for every device this branch builds.
- Add one script that builds and runs each environment with a scripted input
  path, saves screenshots, and writes a single gallery page.
- Make failures obvious: a device whose build fails, crashes, or never reaches
  an expected screen shows as a clear failure in the gallery, not as a missing
  image.
- Provide a small Thai sample EPUB so the reader path shows real Thai text.

## Non-goals

- Running in GitHub Actions (approach C). May come later, after this works locally.
- Pixel-diff / golden-image comparison. The owner reviews images by eye.
- Modeling e-paper timing, ghosting, power, RAM limits, or real touch hardware.
- Forking `crosspoint-simulator` (owner's decision: no extra repo to maintain).
- The Metalio E-Ink 4 Plus: it lives on the separate `thai-eink4plus` branch.

## Anti-goals

- Must not change any device firmware build or `default_envs`. Simulator envs
  are `platform = native` and only build when named with `-e`.
- Must not add simulator-only code paths to firmware that change device behavior.
  Any `#ifdef SIMULATOR` added to `src/` must be a compile fix only.
- Must not leave the owner thinking the simulator replaces hardware testing.
  The gallery page states that it is not e-paper validation.

## Constraints

- Host: macOS (Apple Silicon). SDL2 already installed via Homebrew.
- Simulator: `https://github.com/crosspoint-reader/crosspoint-simulator`,
  integrated per its `sample-platformio-macos.ini`. Pin to a commit for
  reproducible builds.
- Simulator revision (owner's decision): the simulator tracks upstream
  `develop`, and this fork branched around 2026-11-01 (merge-base `42d58daa`).
  Try the latest official commit first. If it does not build against this fork,
  pin the official commit `8dcf00b0` (2026-10-31). Do **not** fork the
  simulator. That commit has all nine device profiles, `CROSSPOINT_SIM_INPUT_SCRIPT`
  (keys, `TAP:`), `CROSSPOINT_SIM_SCREENSHOTS` and `CROSSPOINT_SIM_SD`, but not
  `WAIT:` or `SHOT:`. Small compile-only `#ifdef SIMULATOR` fixes in `src/`
  are allowed. If the gap is bigger than that, stop and report to the owner.
- `fs_/` (the simulator's SD card folder) is already in `.gitignore`.
- The simulator's scripted QA API: `CROSSPOINT_SIM_INPUT_SCRIPT` (key presses,
  `TAP:x,y`, `WAIT:<regex>` on firmware log lines, `SHOT:<path>`),
  `CROSSPOINT_SIM_SD` (SD folder), BMP screenshots.
- The firmware reopens the last book at boot, so each run must start from a
  fresh SD folder.
- Repo rules (`CLAUDE.md`): firmware-handoff applies to the `platformio.ini`
  change; the human approves commits; no push without explicit instruction.

## Approaches considered

- **A. Simulator envs only, manual clicking.** Least work, but checking ~9
  devices by hand before each release is tedious and likely to be skipped.
- **B. A + scripted check over all devices with a screenshot gallery.**
  Chosen by the owner. One command, same path every time, quick visual review.
- **C. B + GitHub Actions on `th-*` tags.** Automatic, but needs headless SDL on
  Linux CI and another layer to maintain. Deferred until B is proven.
- Rejected: the community QEMU full-system emulator
  (`EdenRochmanSharabi/crosspoint-simulator`) only emulates the X4.

## Decisions

### Devices (owner's request: all models; list is builder's mapping)

One simulator env per device env on this branch:

| Device env | Simulator env | Simulator flags |
| --- | --- | --- |
| `default` (X4) | `simulator` | base |
| `default` (X3) | `simulator_x3` | `-DSIMULATOR_DEVICE_X3` |
| `x4pro` | `simulator_x4_pro` | `-DSIMULATOR_DEVICE_X4_PRO` |
| `x4c` | `simulator_x4_classic` | `-DSIMULATOR_DEVICE_X4_CLASSIC` |
| `sticky` | `simulator_sticky` | `-DSIMULATOR_DEVICE_STICKY` |
| `papermono` | `simulator_papermono` | `-DSIMULATOR_DEVICE_PAPERMONO` |
| `metalio_eink4` | `simulator_metalio_eink4` | `-DSIMULATOR_DEVICE_METALIO_EINK4` |
| `eego_a4` | `simulator_eego_a4` | `-DSIMULATOR_DEVICE_EEGO_A4` |
| `picco` | `simulator_picco` | `-DSIMULATOR_DEVICE_PICCO` |

Each env also sets the matching `FREEINK_DEVICE_*` define used by the real device
env, as the simulator samples do. Display-controller variants (UC8179/UC8279)
are left out of the default run. They use the same geometry and capabilities,
so they add build time without changing what a screenshot shows.

### Check path (per device)

```
boot → Home → file browser → open Thai sample book → one page forward → back out → Settings
```

Screenshot at: Home, file browser, book first page, book next page, Settings.

If the pinned simulator has `WAIT:`/`SHOT:`, each step waits on the firmware's
`Entering activity: %s` log line (`src/activities/Activity.cpp:5`) before the
next key or screenshot. On `8dcf00b0` the script uses fixed timings with
generous gaps instead (`CROSSPOINT_SIM_INPUT_SCRIPT` + `CROSSPOINT_SIM_SCREENSHOTS`).
After the run, it checks the captured log for each expected
`Entering activity:` line, in order. A device is marked failed if the build
fails, the process crashes or exits non-zero, an expected activity line is
missing, or a screenshot file is missing. With fixed timings, a slow host can
capture a screen mid-transition. The gallery shows the log check result next to
each image, so a wrong-looking image is easy to tell apart from a real failure.

Input differs by device. Button devices use the scripted key names. Touch-only
or partial-button devices (EEGO A4, Picco, Metalio) use whichever keys the
simulator maps for that profile (see its per-device docs). The script holds one
small per-device input table.

### Thai sample book

A generated EPUB with original Thai text: a few paragraphs, including combining
marks (vowels and tone marks above and below), long unbroken Thai words for line
breaking, and mixed Thai/English with numbers. Generated by a small Python script
in `scripts/`, following the existing `scripts/generate_*_epub.py` pattern, and
copied into each run's fresh `fs_` folder under `/books/`. Use text written for
this project, not copyrighted text.

### Thai UI language

A fresh `fs_` boots in English (`language = 0` is EN, `src/CrossPointSettings.h:428`).
Each run must seed the settings file in its fresh SD folder with the Thai
language, so the screenshots show the Thai UI. Thai glyphs are built into the
firmware (`lib/EpdFont/scripts/convert-builtin-fonts.sh:10`), so no fonts go on
the SD card.

### Output

`build/sim-check/<timestamp>/` containing each device's BMPs (or PNGs converted
with `sips`), its log, and `index.html`: one row per device, one column per
step, a red failure cell with the last log lines when a step fails, and a header
line saying this is a simulator check, not e-paper validation. `build/` is
already outside version control.

### Entry point

`scripts/sim_check.sh` (or `.py`, builder's call), run from the repo root.
Optional arguments: a device subset (e.g. `sim_check.sh x4_pro picco`).
The default is all devices. It prints a one-line pass/fail summary per device
and the path to `index.html`, and exits non-zero if any device failed.

## Left to the builder

- Exact env placement: add to `platformio.ini` behind a clear `; --- simulator ---`
  section, or a separate `platformio.simulator.ini` added via `extra_configs`.
  Prefer whichever keeps the device envs untouched and readable.
- Compile fixes needed for this fork's Thai renderer/font code under the
  simulator. Keep them minimal and guarded. If the auto-patch for
  `GfxRenderer::setOrientation()` does not match this fork, add the documented
  `display.setSimulatorOrientation()` call.
- Exact activity log strings for `WAIT:` (read from `src/`).
- Per-device key/tap sequences.
- Build parallelism and timeout values.

## Verification (for the owner)

1. `scripts/sim_check.sh x4_pro`: one device end to end. Open `index.html` and
   confirm the five screenshots show readable Thai.
2. `scripts/sim_check.sh`: all devices. Summary lists each device as passing.
3. Break something on purpose (e.g. rename the sample book). The affected step
   shows as a red failure, and the script exits non-zero.
4. `pio run -e default` and one other device env still build unchanged.
5. Hardware testing on owned devices is still required before release.
