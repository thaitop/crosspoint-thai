# Testing and Debugging

CrossPoint runs on real hardware, so debugging usually combines local build checks and on-device logs.

## Local checks

Make sure `clang-format` 21+ is installed and available in `PATH` before running the formatting step.
If needed, see [Getting Started](./getting-started.md).

```sh
./bin/clang-format-fix
pio check --fail-on-defect low --fail-on-defect medium --fail-on-defect high
pio run
```

## Simulator check for all devices

The desktop simulator ([crosspoint-simulator](https://github.com/crosspoint-reader/crosspoint-simulator))
runs the firmware on macOS with each device profile. Its environments are in
`platformio.simulator.ini`. Install SDL2 first (`brew install sdl2`).

Check every device:

```sh
uv run -p 3.12 scripts/sim_check.py            # all devices
uv run -p 3.12 scripts/sim_check.py x4_pro picco   # some devices
```

Device names: `x4`, `x3`, `x4_pro`, `x4_classic`, `sticky`, `papermono`,
`metalio_eink4`, `eego_a4`, `picco`.

Each device gets a fresh simulated SD card with the Thai UI,
`test/epubs/thai_sample.epub`, and the Thai reader font copied to
`/.fonts/<family>/`. The font is not in the repository. By default the script
reads `~/Downloads/E-Reader/Fonts/GoogleSans` (a folder of `GoogleSans_<size>.cpfont` files).
Set `SIM_CHECK_FONT_DIR` to use another folder. The script follows the same path on each device:
Home, file browser, two book pages, Home again, Settings. Set
`SIM_CHECK_UI_THEME` to a theme number (8 = Shelf) to seed that home theme. It writes screenshots and logs to
`build/sim-check/<time>/`. Open `index.html` there to review
the screenshots. The script exits with 1 if a build fails, the simulator
crashes or hangs, an expected screen is not reached, a screenshot is missing,
page 2 looks the same as page 1, the reader font is not loaded, or no Thai text
is drawn. It exits with 2 if a device name is unknown or the font folder, the
sample book or `pio` is missing.

To open one device by hand: `pio run -e simulator_x4_pro -t run_simulator`.
A manual run uses `./fs_` as the SD card, which the script does not prepare:
copy the font and books there yourself.

The simulator does not model e-paper timing, ghosting, power, the real RAM
limit, USB mass storage or Bluetooth page turners. It does not replace hardware testing.

Tests for the script: `uvx pytest test/sim_check -v`.
Regenerate the sample book: `uv run -p 3.12 scripts/generate_thai_sample_epub.py`.

## Flash and monitor

Flash firmware:

```sh
pio run --target upload
```

Open serial monitor:

```sh
pio device monitor
```

Optional enhanced monitor:

```sh
python3 -m pip install pyserial colorama matplotlib
python3 scripts/debugging_monitor.py
```

## Useful bug report contents

- Firmware version and build environment
- Exact steps to reproduce
- Expected vs actual behavior
- Serial logs from boot through failure
- Whether issue reproduces after clearing `.crosspoint/` cache on SD card

## Common troubleshooting references

- [User Guide troubleshooting section](../../USER_GUIDE.md#7-troubleshooting-issues--escaping-bootloop)
- [Webserver troubleshooting](../troubleshooting.md)
