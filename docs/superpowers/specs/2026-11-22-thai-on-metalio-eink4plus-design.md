# Thai features on Metalio E-Ink4-Plus — design

## Intent

The owner has a Metalio E-Ink4-Plus (4.7", 1216×684, ESP32-S31). The upstream
"Metalio EINK4 Plus Beta 2" firmware already boots and works on it. The owner
wants every Thai feature from `thai-2.0.0` on this device.

## Goals

- One branch with all 17 Thai commits on top of
  `upstream/feat-metalio-eink4plus` (`ea534874`, 2026-11-19).
- The `metalio_eink4_plus` env builds, and the Thai features work on the Plus.
- The X4 Pro env still builds from the same branch, so one branch can serve both
  devices.

## Non-goals

- New Thai features or redesigns.
- Speaker, microphone, 4G, camera (upstream does not support them yet).
- Sending changes upstream.

## Anti-goals

- A regression of the Thai features on X4 Pro compared with `thai-2.0.0`.
  Upstream behavior changes that come with the new base (about 107 commits and
  SDK pin `99901125`, including gray settle and display changes for all panels)
  are accepted as they are. The owner chose this so one branch serves both
  devices.
- Hand-merged generated files (font headers, i18n) that do not match their
  generator output.
- A cache format change without a version bump. Old caches would then load with
  the wrong layout.

## Constraints

- The Plus SDK pin (`freeink-sdk@99901125`) comes with the upstream branch. It
  is not changed in this work.
- The Plus env uses pioarduino 61.04.00-RC1 (ESP-IDF 6.1) and `-std=gnu17`.
  Thai code must compile with this toolchain too.
- Hardware testing is done by the owner. Agents only build and run host tests.

## Approaches

| | Approach | Trade-off |
|---|---|---|
| **A (chosen)** | New branch from `upstream/feat-metalio-eink4plus`; replay the 17 Thai commits on top | Clear history, Thai commits stay reviewable and can be replayed again when upstream updates the Plus branch; conflicts are solved per commit |
| B | Merge the Plus branch into `thai-2.0.0` | One conflict round, but mixed history and a new merge on every upstream update |
| C | Rebase Thai onto `upstream/develop`, then add Plus | Cleanest after Plus lands in `develop`; double work today |

The owner chose A.

## Known overlap

These 23 files were changed by both the Thai commits and the Plus branch
(since merge base `42d58daa`), so conflicts are expected there:

- Text rendering: `lib/GfxRenderer/GfxRenderer.cpp`, `lib/EpdFont/EpdFontData.h`,
  `lib/Epub/Epub/ParsedText.cpp`, `lib/Epub/Epub/Section.cpp`
- Generated font data: `lib/EpdFont/builtinFonts/ubuntu_8_regular.h`,
  `lib/EpdFont/scripts/fontconvert.py`, `convert-builtin-fonts.sh`
- Settings/UI: `CrossPointSettings.*`, `SettingsList.*`,
  `SettingsActivity.cpp`, `HomeActivity.*`, `UITheme.*`, `main.cpp`
- Sleep screen: `SleepActivity.*` (upstream added an 8-bit grayscale cover
  path; Thai added the calendar sleep screen)
- Other: `english.yaml`, `USER_GUIDE.md`, `docs/file-formats.md`,
  `test/CMakeLists.txt`

## Decisions (builder's calls, owner may override)

- **Branch name:** `thai-eink4plus`, created from
  `upstream/feat-metalio-eink4plus`. `thai-2.0.0` stays as it is until the new
  branch is confirmed on both devices.
- **Commit order:** keep the original Thai commit order. Each commit must build
  on its own after conflict resolution.
- **Generated files:** when a conflict is in generated output, take the upstream
  version and run the generator again with the Thai inputs. Do not merge the
  generated text by hand. This also keeps the upstream change that made the
  digit 1 thinner in the 8 pt font.
- **Gray rendering:** upstream now draws text with gray anti-aliasing and a
  "gray settle" pass. Thai mark stacking must give the same mark positions in
  the B/W and gray passes.
- **Calendar sleep screen vs. gray cover:** keep both. The Thai calendar is one
  more sleep-screen mode; the 8-bit gray cover path stays as upstream wrote it.
- **Cache versions:** where Thai and upstream both bumped a section/layout
  cache version, use a new value above both.
- **Layout on 1216×684:** the Thai home themes and calendar were made for
  480×800. This port only checks that they draw without crashing or clipping
  on the Plus. If one crashes, clips or overflows, a small separate fix commit
  is in scope. Visual tuning for the larger, denser screen is a follow-up task
  after the owner's hardware test.
- **Thai text in gray mode:** Thai glyphs follow the same render path as all
  other text on each panel. On the Plus this means gray anti-aliasing when
  upstream uses it. No Thai-only B/W fallback.
- **Per-commit build:** each replayed commit must build `metalio_eink4_plus`.
  `x4pro` and host tests run at the end, and again on any commit that changes
  shared rendering code.
- **Settings from Beta 2:** a device that has Beta 2 settings must boot
  without a reset. New Thai fields fall back to defaults when they are missing.
  The plan checks this against the upstream settings format.

## Verification

- Builds: `metalio_eink4_plus` and `x4pro`.
- Host tests: all tests under `test/`, including `thai_word_break`.
- Owner's hardware test on the Plus: Thai EPUB line breaking, mark stacking in
  B/W and gray, Thai UI language, home themes, reading log, calendar sleep
  screen, and the cover sleep screen.
- Owner's quick check on X4 Pro: same list, compared with `thai-2.0.0`.

## Left to the builder

- Resolving each conflict, based on the rules above.
- Whether a Thai commit needs a small follow-up fix commit after replay,
  instead of changing it heavily during the replay.
