# Heavier UI Text on Plus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make regular-weight UI text on scaled-UI boards (Metalio E-Ink4-Plus) as heavy as the owner-approved build (Medium fonts, 30/40/55% levels).

**Architecture:** Swap the Regular 2-bit UI faces' Ubuntu and Noto Sans Thai sources for their Medium cuts, with a new `fontconvert.py` option that limits kerning so the Medium face fits the 255-class format; then darken the coverage-to-level map in `ScaledFont`.

**Tech Stack:** Python font generator (`freetype-py`, `fontTools`), C++ firmware (PlatformIO, pioarduino core 6.2.0 for this branch).

**Spec:** `docs/superpowers/specs/2026-11-22-plus-ui-text-weight-design.md`

## Global Constraints

- Work only in worktree `../crosspoint-thai-eink4plus`, branch `thai-eink4plus`. `thai-2.0.0` is not touched.
- Build: `PLATFORMIO_CORE_DIR=~/.platformio-eink4plus ~/.platformio-eink4plus/penv/bin/pio run -e <env>`.
- Run the generator: `cd lib/EpdFont/scripts && uv run -q --with freetype-py --with fonttools python fontconvert.py …`.
- Change only `ubuntu_{8,10,12}_regular_2bit.h`. Bold 2-bit and all 1-bit headers must stay byte-identical.
- Kerning kept for: U+0000–U+024F, U+0E00–U+0E7F, U+2000–U+206F.
- Level thresholds: level 1 at ≥30% of `FULL`, level 2 at ≥40%, level 3 at ≥55% (changed from 25/35/45 after the owner's device check).
- Commits need the owner's approval (CLAUDE.md); end messages with the attribution line.

## Review Focus

- Running `convert-builtin-fonts.sh` from scratch must regenerate every other header unchanged → Task 2 Step 4 diffs the full set.
- A face without `--kern-intervals` must be byte-identical to today's output → Task 1 Step 3.
- Thai tone marks over upper vowels (e.g. "นั้น", "ที่") must still stack correctly with the Medium Thai face → owner check, Task 4.
- X4 Pro firmware must still fit and be unchanged in look → Task 3 builds `x4pro`.
- Bold UI text gets darker too (the level map is shared) → owner check, Task 4.

---

### Task 1: `--kern-intervals` option in `fontconvert.py`

**Files:**
- Modify: `lib/EpdFont/scripts/fontconvert.py` (argument list at ~line 19-29; kerning collection after line 559, before the "pairs extracted" print)

**Interfaces:**
- Produces: CLI option `--kern-intervals MIN,MAX` (hex, repeatable, `action="append"`, same `min,max` parsing as `--additional-intervals`). When given, drop every `kern_map` pair unless both code points fall inside one of the intervals. When absent, behavior is unchanged.

- [ ] **Step 1: Add the option and the filter.** Help text: "Keep kerning only for pairs whose two code points are both inside these intervals (min,max). Repeatable." Print `kerning: N pairs kept by --kern-intervals` to stderr when it applies.

- [ ] **Step 2: Show the failure it fixes.** Using the Medium sources (Task 2 Step 1 adds them; run from the scratchpad copies if Task 2 is not done yet), generate `ubuntu_10_regular_2bit` without the option. Expected: `Error: 264 right classes exceed the uint8 column range`. With `--kern-intervals 0x0000,0x024F --kern-intervals 0x0E00,0x0E7F --kern-intervals 0x2000,0x206F`, expected: success and well under 255 classes (the spike gave 108 left / 123 right).

- [ ] **Step 3: No-option output unchanged.** Regenerate `ubuntu_8_regular_2bit` with today's Regular command (copy it from `convert-builtin-fonts.sh`) to a temp file and `cmp` it with the committed header. Expected: identical.

- [ ] **Step 4: Commit** (after owner approval) — `feat: add kerning interval filter to fontconvert`.

### Task 2: Medium sources for the Regular 2-bit UI faces

**Files:**
- Create: `lib/EpdFont/builtinFonts/source/Ubuntu/Ubuntu-Medium.ttf` (https://github.com/google/fonts/raw/main/ufl/ubuntu/Ubuntu-Medium.ttf)
- Create: `lib/EpdFont/builtinFonts/source/NotoSansThai/NotoSansThai-Medium.ttf` (https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSansThai/hinted/ttf/NotoSansThai-Medium.ttf)
- Modify: `lib/EpdFont/scripts/convert-builtin-fonts.sh:93-108` (2-bit loop)
- Modify (generated): `lib/EpdFont/builtinFonts/ubuntu_{8,10,12}_regular_2bit.h`

**Interfaces:**
- Consumes: `--kern-intervals` from Task 1.

- [ ] **Step 1: Add the two font files.** Both licences are already in those folders (`UFL.txt`, `OFL.txt`).

- [ ] **Step 2: Change the 2-bit loop.** For `Regular` entries only: Ubuntu source `Ubuntu-Medium.ttf`, Thai source `NotoSansThai-Medium.ttf`, plus the three `--kern-intervals` above. NotoSans, Hebrew, Arabic and `Ubuntu-Vietnamese-Regular.ttf` stay as they are; `Bold` entries are unchanged. Update the loop's comment in one line: the Regular faces use Medium sources so scaled UI text keeps solid stems.

- [ ] **Step 3: Run the whole script.** `cd lib/EpdFont/scripts && uv run -q --with freetype-py --with fonttools bash convert-builtin-fonts.sh`. Expected: finishes, including `verify_compression.py`.

- [ ] **Step 4: Check that only the intended headers changed.** `git status --short lib/EpdFont/builtinFonts`. Expected: only the two new `.ttf` files and `ubuntu_{8,10,12}_regular_2bit.h`. If anything else changed, stop and report it.

- [ ] **Step 5: Commit** (after owner approval) — `feat: use Medium weight for scaled UI regular fonts`.

### Task 3: Darker level map in `ScaledFont`

**Files:**
- Modify: `lib/GfxRenderer/ScaledFont.cpp:160-168`

- [ ] **Step 1: Replace `INK_FROM` and the linear level formula** with the three thresholds from Global Constraints (as `constexpr int` named for what they are, e.g. `LIGHT_FROM`, `DARK_FROM`, `BLACK_FROM`). Rewrite the comment above it (short): gray UI frames show level 1/2 as edge gray and only level 3 as black, so the thresholds were tuned on the Plus panel to give solid stems.

- [ ] **Step 2: Format.** `uvx --from clang-format clang-format -i lib/GfxRenderer/ScaledFont.cpp`.

- [ ] **Step 3: Build both boards.** Envs `metalio_eink4_plus` and `x4pro`. Expected: SUCCESS for both; Plus flash usage about 92.0% (spike value).

- [ ] **Step 4: Commit** (after owner approval) — `fix: darken scaled UI glyph levels`.

### Task 4: Hardware check and handoff

- [ ] **Step 1: Owner flashes the Plus** with `-t upload`, or with the esptool command used during the spike.
- [ ] **Step 2: Owner checks** the settings screen, file browser and a popup, in Thai and English. It must look right to the owner, with no filled counters (`e a 8`, ศ ห) and correct Thai mark stacking ("นั้น", "ที่").
- [ ] **Step 3: Run the `firmware-handoff` skill** (CLAUDE.md requires it) before calling the work ready.
