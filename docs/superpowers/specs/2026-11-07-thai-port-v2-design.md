# Port Thai support and custom home themes to CrossPoint 2.0.0

Date: 2026-11-07
Owner: thaitop (Uthai Moolpak)

## Intent

The owner reads Thai books on an Xteink X4 Pro. Their Thai work lives in a
separate private repo, `crosspoint-x4pro-th` (local:
`/Users/thaitop/Documents/GitHub/crosspoint-x4pro-th`, branch
`feature/calendar-theme`). That repo was made from one squashed commit on top
of upstream `1.6.5`, so it shares no history with upstream. Every upstream
release must be ported by hand.

Upstream has released `2.0.0`. The owner wants the Thai features on 2.0.0, in
this repo (`crosspoint-thai`, a real fork of `crosspoint-reader`), so that
later upstream releases can be taken by rebasing a small set of Thai commits
onto the new tag instead of porting by hand again.

## Goals

1. Work on a new branch made from tag `2.0.0` (suggested name `thai-2.0.0`,
   matching the earlier `thai-1.6.5` branch). Not on `master`.
2. Keep Thai work as a few focused commits, one per topic, so each one
   rebases cleanly on future tags.
3. Support every board that upstream 2.0.0 supports. Keep upstream's
   `platformio.ini` environments and CI. Do not copy the X4 Pro-only trimming
   from `crosspoint-x4pro-th`.
4. Keep all upstream UI themes (Classic, Lyra, Lyra Extended, RoundedRaff,
   Cover Grid). Add the owner's three home themes as new choices.
5. Port these features, keeping the behavior they have in
   `crosspoint-x4pro-th`:
   - **Thai mark placement.** Above-base Thai marks (upper vowels, tone marks,
     MAI HAN-AKAT, etc.) stack over each other, tone marks clear SARA AM's
     nikhahit, and marks shift left over tall ascenders (PO PLA, FO FA,
     FO FAN). Source: `thaiMark` in `lib/EpdFont/EpdFontData.h`, its use in
     `lib/EpdFont/EpdFont.cpp` (`getTextBounds`) and in
     `lib/GfxRenderer/GfxRenderer.cpp` (`thaiMarkOffset`, `drawText`,
     `drawTextRotated90CW`). Commits `93bc847` (initial), `1e47aa6`.
     This works at draw time, so it must apply to SD card fonts too.
   - **Thai word breaking.** `lib/ThaiWordBreak/` (dictionary-based, generated
     `thai-dict.h` from `scripts/generate_thai_dictionary.py`), used:
     - in EPUB layout: `gaplessBreakByteOffsets` in
       `lib/Epub/Epub/ParsedText.cpp` (falls back to Thai breaks when the CJK
       break scan finds none);
     - in UI text: `GfxRenderer::truncatedText` must not cut a letter from
       its marks (`ThaiWordBreak::clusterStart`), and
       `GfxRenderer::wrappedText` must break long Thai runs at word breaks,
       else at a cluster start.
     - Host test: `test/thai_word_break/`, plus the CMake link changes in
       `test/chapter_html_slim_parser/` and `test/korean_line_breaking/`, and
       the case in `test/combining_marks/CombiningMarkAnchorTest.cpp`.
   - **Thai UI language.** `lib/I18n/translations/thai.yaml`, completed for
     every string that 2.0.0 adds. The Thai category label fix from
     `efa7588` stays. Follow `docs/i18n.md` "Adding a New Language" and the
     repo translation workflow.
   - **Thai glyphs in the built-in UI fonts.** Add the Thai block
     U+0E01–U+0E5B from Noto Sans Thai (Regular/Bold, OFL, under
     `lib/EpdFont/builtinFonts/source/NotoSansThai/`) to the Ubuntu UI fonts
     and `ubuntu_8_regular` (2.0.0 commit `cf50ce73` made Ubuntu the small UI font; there is no `notosans_8_regular` any more), via `lib/EpdFont/scripts/convert-builtin-fonts.sh`,
     then regenerate those headers. See commit `1e2965c`.
   - **Thai-safe slider readout.** The slider dialog
     (`src/components/UiSliderDialog.h`) switches its readout to
     `NOTOSANS_18`, which has no Thai. Only switch when that font covers
     every codepoint of the readout. Commit `5c4ba24`.
   - **Home themes Ledger, Today and Calendar, and the reading log.**
     - `src/components/LedgerHome.*`, `TodayHome.*`, `CalendarHome.*`,
       shared `HomeWidgets.*`; integration in
       `src/activities/home/HomeActivity.*`, `src/components/UITheme.*`,
       `src/SettingsList.h`, `src/CrossPointSettings.*`, and new strings.
     - Reading log: `src/ReadingLogStore.*`, stored in
       `/.crosspoint/reading_log.json`; loaded in `src/main.cpp`; reading
       time credited in `src/activities/reader/ReaderActivity.*`
       (`creditReadingTime`, gaps capped at 5 minutes, saved on exit,
       nothing logged before the clock is set).
     - Commits `614c819`, `20277bb`, `79f5ea0`, `bb2eeb5`. Behavior is
       described in those commit messages.
     - On non-home screens these themes use Lyra styling, as they do now.
6. Bump `SECTION_FILE_VERSION` in `lib/Epub/Epub/Section.cpp` (73 in 2.0.0)
   for the layout change from Thai word breaking, with a version comment.

## Non-goals

- No Thai glyphs in the built-in reader fonts. The owner reads Thai with
  their own `.cpfont` font on the SD card. Do not port commit `50b05d8`
  ("embed Thai glyphs in built-in reader fonts") or the reader-font part of
  `convert-builtin-fonts.sh`. Known trade-off, accepted by the owner: a Thai
  book with no Thai SD font shows missing-glyph boxes.
- Do not port commit `576671d` (removal of Lyra Extended, RoundedRaff, Cover
  Grid, the `uiTheme` → `theme` key rename, and its renumbering migration).
- Do not port the X4 Pro-only changes: trimmed `platformio.ini`, trimmed CI
  and release workflows, removed `nix/`, README and USER_GUIDE edits.
- No new features or redesigns of the three themes during the port.
- No change to the `.cpfont` format. It is still version 4 in both 1.6.5 and
  2.0.0 (`lib/EpdFont/SdCardFont.h:21`), so existing SD fonts keep working.
- Do not change or delete `crosspoint-x4pro-th`. It is only the source.

## Anti-goals (a failure even if it builds)

- Thai changes spread across many unrelated commits or mixed with upstream
  cleanup, so that a future rebase becomes a hand port again.
- Any upstream theme removed, renamed, or its stored setting value changed.
- A board that builds on upstream 2.0.0 no longer builds or no longer fits
  its flash partition.
- Thai mark or word-break behavior that differs from `crosspoint-x4pro-th`.

## Constraints

- Follow this repo's `CLAUDE.md` and `.agents/rules/`. In particular: cite
  evidence, justify new heap use, `firmware-handoff` before declaring ready,
  no commits without explicit owner approval, no push without an explicit
  push instruction.
- ESP32-C3 boards (X3/X4) are the resource baseline. Thai glyphs in UI fonts
  and the Thai dictionary add flash. Build every board environment in
  `platformio.ini` and report flash use per board before and after.
- Theme setting: upstream stores `uiTheme` with Classic=0, Lyra=1,
  Lyra Extended=2, RoundedRaff=3, Cover Grid=4. New themes must take new
  values after these. Upstream's `homeThemeValues()`
  (`src/SettingsList.h:270`) hides Cover Grid on boards without PSRAM by
  dropping the last list entry. Because the menu index is the stored value,
  appending new themes after Cover Grid breaks that trick. The builder must
  pick a way that keeps stored values stable and still hides Cover Grid
  without PSRAM.
- The new home screens must work on every supported board: button-only and
  touch input, and each board's screen size and orientation. Calendar's tap
  on a past day is a touch extra; button-only boards must still be usable.
- Hardware testing is the owner's job and only on the X4 Pro. Other boards
  get build checks only; say so in the handoff.

## Decided

- Repo: `crosspoint-thai`, branch from tag `2.0.0`.
- All boards, like upstream.
- Keep all upstream themes; add Ledger, Today, Calendar.
- No Thai in built-in reader fonts; Thai in UI fonts only.
- Translations: the builder drafts Thai for every string 2.0.0 added
  (146 keys), gives the owner a list to review and edit, and commits only
  after the owner approves. Untranslated keys fall back to English
  meanwhile.
- Flash fit: if any board no longer fits its partition, stop and report
  the numbers to the owner. Do not add per-board flags or other fixes
  without the owner's decision.

## Calls made by the designer (owner to check)

- New theme values are Ledger=5, Today=6, Calendar=7 under upstream's
  `uiTheme` key. These match the older `crosspoint-x4pro-th` numbers.
  No migration from that repo's `theme` key: after flashing, the owner
  selects the theme again once. `/.crosspoint/reading_log.json` keeps its
  path and format, so reading history carries over.
- Port from the final state of `feature/calendar-theme` (the word breaker
  is in `lib/ThaiWordBreak/` there), not commit by commit.
- On boards other than the X4 Pro, the three home layouts adapt to the
  screen size from the renderer, like the current code. No per-board
  design work. Evidence for those boards is build-only, and the handoff
  says so.

## Left to the builder

- Exact commit split (suggested: mark placement; word break library and
  EPUB use; UI text wrapping/truncation; UI fonts + Thai translation +
  slider fix; reading log; each home theme or the three together).
- How to insert the Thai break call into the 2.0.0 `ParsedText.cpp`, which
  changed a lot (perf commit `3e2af487` reworked the CJK break scan).
- How to hook the theme homes into the 2.0.0 `HomeActivity`, `UITheme` and
  `SettingsList.h`.
- Whether the theme-setting fix above needs a small change to upstream code.

## Verification

- Host tests (`test/`), including `thai_word_break` and `combining_marks`.
- Build every `platformio.ini` environment; compare flash and static RAM
  with plain 2.0.0.
- Formatting per the repo testing workflow.
- Owner tests on X4 Pro: Thai UI, a Thai EPUB with an SD Thai font (marks,
  line breaks), each upstream theme, and Ledger, Today, Calendar, including
  reading time after a reading session.
