# Thai on Metalio E-Ink4-Plus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replay the Thai commits from `thai-2.0.0` on top of `upstream/feat-metalio-eink4plus`, so one branch builds Thai firmware for both the E-Ink4-Plus and the X4 Pro.

**Architecture:** New branch `thai-eink4plus` in its own worktree, created from `upstream/feat-metalio-eink4plus` (`ea534874`). Cherry-pick the Thai commits in their original order, in four groups. Each group ends with builds and host tests. Conflicts are resolved by the rules in the spec. Generated files are regenerated, not hand-merged.

**Tech Stack:** C++ (PlatformIO, ESP32-S3 / ESP32-S31 with pioarduino 61.04.00-RC1), host GoogleTest via CMake, Python generators.

**Spec:** `docs/superpowers/specs/2026-11-22-thai-on-metalio-eink4plus-design.md`

## Global Constraints

- Base: `upstream/feat-metalio-eink4plus` at `ea534874`. Do not update it to a newer upstream commit during this plan.
- SDK pin stays `freeink-sdk@99901125` (from the base branch). Do not change it.
- `thai-2.0.0` and its worktree (`/Users/thaitop/Documents/GitHub/crosspoint-thai`) are not modified.
- Worktree: `/Users/thaitop/Documents/GitHub/crosspoint-thai-eink4plus`. Run all commands there.
- Every commit needs the owner's approval. Ask once before Task 1 for the whole replay set. Never push.
- Commit messages: keep the original Thai message. For follow-up fixes use `fix:` and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Build: `~/.platformio/penv/bin/pio run -e <env> > $SCRATCH/<task>-<env>.log 2>&1; echo exit=$?`. Report env, exit status, the RAM/Flash lines and the log path only.
- Host tests: `uvx cmake -S test -B build/host-test && uvx cmake --build build/host-test -j && (cd build/host-test && uvx --from cmake ctest --output-on-failure)`.
- Formatting: `./bin/clang-format-fix -g` before each commit that touches C/C++.
- i18n regenerate: `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.
- Font regenerate: `lib/EpdFont/scripts/convert-builtin-fonts.sh`, then `lib/EpdFont/scripts/build-font-ids.sh`, in a Python ≥3.10 env (`uv venv -p 3.12`).
- `SECTION_FILE_VERSION` becomes **76** (Thai used 74, upstream used 75).
- Settings are JSON with named keys. A missing Thai key must load as its default. No settings reset.
- Thai glyphs use the same render path as other text, including the gray anti-aliasing path. No Thai-only B/W fallback.
- Hardware tests are done by the owner. The `firmware-handoff` skill must be completed before the branch is declared ready.

## Review Focus

1. **Thai marks in the gray pass:** Thai above-base marks on the Plus must sit in the same place in the gray anti-aliasing pass as in the B/W pass. The owner checks this on the device (Task 5). Task 1 Step 4 checks in code that the mark offset is applied before the glyph is drawn, so every draw path uses it.
2. **Section caches from Beta 2:** caches built by Beta 2 (version 75) must rebuild, not load. Task 1 Step 5 sets version 76.
3. **Settings from Beta 2:** a `settings.json` written by Beta 2 must load. Thai enum values must not reuse values that upstream added. Task 3 Step 3 checks the enums. Task 5 checks booting on the device.
4. **Regenerated UI fonts:** they must keep upstream glyph changes (the thinner digit 1 in 8 pt) and only add the Thai ranges. Task 2 Step 3 checks this.
5. **Thai screens at 1216×684:** home themes and the calendar sleep screen must not assume 480×800. Task 3 Step 4 and Task 4 Step 4 grep for fixed sizes. Task 5 checks on the device in all four orientations.

---

### Task 0: Worktree and baseline

**Files:** none changed.

- [ ] **Step 1: Create the worktree and branch**

```bash
cd /Users/thaitop/Documents/GitHub/crosspoint-thai
git fetch upstream
git worktree add -b thai-eink4plus ../crosspoint-thai-eink4plus ea534874
cd ../crosspoint-thai-eink4plus
git submodule update --init --recursive
git submodule status
```
Expected: `freeink-sdk` at `99901125`, `libs/display/epdiy` initialized.

- [ ] **Step 2: Build the baseline**

Build `metalio_eink4_plus` and `x4pro`. Expected: both exit 0. Write down the RAM/Flash lines, so later tasks can compare against them.

- [ ] **Step 3: Run host tests on the baseline**

Expected: all pass. If a test fails on the clean base, record it as already failing. Do not fix it in this plan.

---

### Task 1: Thai text shaping and line breaking

Commits, in order: `d530b7a9` (mark stacking), `74c552b8` (dictionary word break), `0278f9bc` (EPUB word break), `e63b4ee6` (UI text clusters).

**Files (expected conflicts):** `lib/GfxRenderer/GfxRenderer.cpp`, `lib/EpdFont/EpdFontData.h`, `lib/Epub/Epub/ParsedText.cpp`, `lib/Epub/Epub/Section.cpp`, `docs/file-formats.md`, `test/CMakeLists.txt`.

**Interfaces:**
- Produces: `ThaiWordBreak` library (`lib/ThaiWordBreak/ThaiWordBreak.h`). Mark stacking in `EpdFont`/`GfxRenderer::drawText`. `SECTION_FILE_VERSION = 76`.

- [ ] **Step 1: Cherry-pick the four commits**

`git cherry-pick d530b7a9 74c552b8 0278f9bc e63b4ee6`. Resolve each conflict before you continue. In `GfxRenderer.cpp`, keep every upstream addition (gray glyph path in `drawGlyphBitmap`, `insertFont`, `resolveVisualText` changes). Add the Thai hunks on top of them.

- [ ] **Step 2: In `test/CMakeLists.txt`, keep both sides' test entries**

Keep upstream's new tests (for example `xtc_parser`, `library_builder`) and add `thai_word_break`.

- [ ] **Step 3: Run host tests**

Expected: all pass, including `combining_marks`, `thai_word_break`, `korean_line_breaking` and `em_dash_line_breaking`.

- [ ] **Step 4: Check that the mark offset reaches every glyph draw path**

Read `GfxRenderer::drawText` and `drawTextRotated90CW` after the merge. The Thai mark offset must be applied to the glyph position before the call to `drawGlyphBitmap` (or its gray variant), not inside one draw branch only. If the upstream gray path draws glyphs from a separate loop, apply the same offset there.

- [ ] **Step 5: Set `SECTION_FILE_VERSION = 76`**

Set it in `lib/Epub/Epub/Section.cpp`. Add a one-line history comment in the existing style: "76: Thai word breaking on top of 75". Update the version note in `docs/file-formats.md` to match.

- [ ] **Step 6: Build and commit**

Build `metalio_eink4_plus` and `x4pro`; both must exit 0. Then run `./bin/clang-format-fix -g`. If Steps 4–5 changed code, put that in a follow-up commit `fix: bump section cache to 76 for Thai on Plus base`.

---

### Task 2: Thai glyphs in UI fonts

Commits: `6cbce126` (Thai glyphs in UI fonts), `fc40bfa6` (slider readout font).

**Files:** `lib/EpdFont/builtinFonts/ubuntu_{8,10,12}_*.h` (generated), `lib/EpdFont/scripts/convert-builtin-fonts.sh`, `lib/EpdFont/scripts/fontconvert.py`, `src/components/UiSliderDialog.h`, `src/activities/util/IntervalSelectionActivity.cpp`, `src/activities/reader/EpubReaderPercentSelectionActivity.cpp`.

- [ ] **Step 1: Cherry-pick `6cbce126`**

For each conflict in a generated `ubuntu_*.h`, take the base branch's version with `git checkout --ours <file>`. During a cherry-pick, "ours" is the base branch and "theirs" is the Thai commit. Merge `convert-builtin-fonts.sh` and `fontconvert.py` by hand, keeping both sides' changes.

- [ ] **Step 2: Regenerate the UI fonts**

Run the font generator commands from Global Constraints. Then run `git cherry-pick --continue`.

- [ ] **Step 3: Check the regenerated fonts**

`git diff ea534874 -- lib/EpdFont/builtinFonts/ubuntu_8_regular.h` must show only added Thai interval/glyph data (U+0E00–U+0E7F). It must not change existing Latin glyphs. In particular, the upstream thinner digit '1' bitmap stays the same.

- [ ] **Step 4: Cherry-pick `fc40bfa6`**

Run the host tests, build both envs, and commit.

---

### Task 3: Reading log, home themes, Thai UI language

Commits, in order: `c1942fbd` (reading log), `6573ff4b` (Ledger/Today/Calendar themes), `d7d30215` (Thai UI language), `314a36e9` (docs), `e63f3b4e` (week-log fix), `e4bcf563` (calendar shading).

**Files (expected conflicts):** `src/main.cpp`, `src/CrossPointSettings.{h,cpp}`, `src/SettingsList.{h,cpp}`, `src/activities/settings/SettingsActivity.cpp`, `src/activities/home/HomeActivity.{h,cpp}`, `src/components/UITheme.{h,cpp}`, `lib/I18n/translations/english.yaml`.

**Interfaces:**
- Produces: `UI_THEME::LEDGER = 5`, `TODAY = 6`, `CALENDAR = 7` and `UI_THEME_COUNT`. `ReadingLogStore` (`src/ReadingLogStore.h`). `lib/I18n/translations/thai.yaml`.

- [ ] **Step 1: Cherry-pick the six commits**

Keep every upstream setting field (for example `doublePwrBtn`, `sideButtonFollowOrientation`, `vocabularyHistoryEnabled`, `showFileExtensions`, `libraryRecentOpenedOnly`) and every upstream `SettingsList` entry. Add the Thai ones next to them.

- [ ] **Step 2: Regenerate i18n**

After each commit that touches `*.yaml`, regenerate with the i18n command from Global Constraints. `thai.yaml` must contain every key that `english.yaml` has. Upstream added new keys, so translate those into Thai too. In that case, add a follow-up commit `fix: translate new upstream strings to Thai`.

- [ ] **Step 3: Check the enum values**

In `src/CrossPointSettings.h`, the values in `UI_THEME` and in the sleep-screen enum must be unique. They must not repeat a value that upstream added after `42d58daa`. If they collide, move the Thai values after the upstream ones, and make loading clamp an unknown value to the default.

- [ ] **Step 4: Check the theme code for fixed sizes**

Run `grep -nE '\b(480|800)\b' src/components/{CalendarHome,LedgerHome,TodayHome,HomeWidgets}.cpp`. Any match that is a screen size must use `renderer.getScreenWidth()` / `getScreenHeight()` instead, in a separate fix commit.

- [ ] **Step 5: Run host tests and build**

Run the host tests and build both envs.

---

### Task 4: Thai calendar sleep screen

Commits, in order: `02ad9a25`, `7a34f2a0` (docs), `d885d40e` (lunar calendar), `1cc52f01` (digits font), `54502c93` (sleep screen). Then cherry-pick the design doc commit `4a7c11ca` from `thai-2.0.0`, together with this plan's commit.

**Files (expected conflicts):** `src/activities/boot_sleep/SleepActivity.{h,cpp}`, `src/CrossPointSettings.h`, `src/SettingsList.cpp`, `lib/EpdFont/builtinFonts/all.h`, `lib/EpdFont/scripts/*`, `src/fontIds.h`, `src/main.cpp`, `USER_GUIDE.md`, `test/CMakeLists.txt`.

- [ ] **Step 1: Cherry-pick the commits**

For `1cc52f01`, regenerate `ubuntu_digits_bold.h`, `all.h` and `src/fontIds.h` with the font generator commands. Do not hand-merge them.

- [ ] **Step 2: Merge `SleepActivity`**

Keep upstream's 8-bit grayscale cover path unchanged. Add `THAI_CALENDAR` and `THAI_CALENDAR_DARK` as extra branches next to it.

- [ ] **Step 3: Run host tests**

Expected: `thai_calendar` passes, and `python3 -m pytest test/test_thai_lunar_table.py` passes.

- [ ] **Step 4: Check the sleep screen for fixed sizes**

Run `grep -nE '\b(480|800)\b' src/activities/boot_sleep/ThaiCalendarSleepScreen.cpp`. Fix any match as in Task 3 Step 4.

- [ ] **Step 5: Build and commit**

Build both envs and commit.

---

### Task 5: Handoff and device test

- [ ] **Step 1: Run the final checks**

Build `metalio_eink4_plus`, `x4pro`, `default` and `slim`. Run the host tests and `./bin/clang-format-fix -g`. Expected: all green, and `git status` is clean.

- [ ] **Step 2: Run the `firmware-handoff` skill**

Run `firmware-handoff` (`.agents/skills/firmware-handoff/SKILL.md`) on `ea534874..thai-eink4plus`. Then give the owner the RAM/Flash delta against the Task 0 baseline.

- [ ] **Step 3: Owner tests on the Plus**

The owner flashes `metalio_eink4_plus` and checks each of these:
- The device boots with the Beta 2 settings kept.
- An old book's cache rebuilds.
- Thai EPUB line breaks fall between words.
- Mark stacking is correct in B/W and gray.
- The Thai UI language works.
- The Ledger, Today and Calendar themes work.
- The reading log records time.
- The calendar sleep screen (light and dark) and the cover sleep screen work.
- The checks above hold in portrait, inverted, landscape CW and landscape CCW.

- [ ] **Step 4: Owner checks on the X4 Pro**

The owner repeats Step 3 on the X4 Pro and compares with `thai-2.0.0`.

- [ ] **Step 5: Fix what the owner reports**

Fix any crash, clipping or overflow from Steps 3–4 in a separate `fix:` commit. The owner approves each fix. Visual tuning for the larger screen goes to a new spec.
