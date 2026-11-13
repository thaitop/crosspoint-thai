# Thai calendar sleep screen

Date: 2026-11-13
Owner: thaitop (Uthai Moolpak)

## Intent

The owner saw a "Chinese almanac" standby screen on another e-reader. That
screen shows a big date, the lunar date, the zodiac year, and a box of the
day's lucky and unlucky activities. The owner wants a Thai version as a new
sleep screen. It is a small fun extra ("gimmick"): something nice to see
when they glance at the sleeping device. It does no other job.

Text must use fonts already built into the firmware. No hand-drawn or
"wobbly" digits like the "sloppy clock" in the same reference.

## What the screen shows

Portrait, top to bottom. The approved mockup is
`.superpowers/brainstorm/16728-1794565793/content/thai-calendar-v4.html`
(drawn at half of 480×800). Example values are for Friday 13 Nov 2026.

1. Header: `พ.ศ. 2569 · ปีมะเมีย` (Buddhist Era year and Thai zodiac
   year), then `พฤศจิกายน  วันศุกร์` (Thai month name and weekday).
2. Day of month, very large and bold: `13`.
3. Moon picture, drawn as the real phase for that day. The lit part and the
   dark part are split by a curved edge (an ellipse arc), not a straight
   line or a fixed set of icons. Waxing (ข้างขึ้น) is lit on the right,
   waning (ข้างแรม) on the left.
4. Thai lunar date, bold: `ขึ้น 4 ค่ำ เดือน 12`.
5. Week strip: the current week, Sunday to Saturday, seven boxes with the
   short Thai weekday and the day of month. Today's box is filled black.
   No Buddhist holy day (วันพระ) marks.
6. Next important day, at the bottom under a rule: a small label
   `วันสำคัญที่กำลังมาถึง`, the name in bold (`ลอยกระทง`), then
   `24 พ.ย. · อีก 11 วัน`. Only one day is shown. On the day itself, show
   it as today (builder picks the wording, e.g. `วันนี้`).

Layout rules from the owner:

- All numbers are Arabic digits (0–9), including the B.E. year and the
  lunar day/month. No Thai digits.
- The content must fill the whole screen height. The owner rejected an
  earlier layout because the bottom was empty. Spread the sections over the
  full height; do not add more content to fill space.
- Screen text is Thai on this screen regardless of the UI language setting
  (designer's call, see below).

## Goals

- A new choice in Settings → Display → Sleep Screen that shows this screen.
- Correct Thai lunar date (ข้างขึ้น/ข้างแรม, ค่ำ, เดือน) and correct moon
  phase, computed on the device with no network.
- Correct next important Thai day with a day count.

## Non-goals

- No live clock and no time of day. E-ink is drawn once when the device goes
  to sleep and does not update while it sleeps. If the device sleeps over
  midnight, the screen shows the old date. That is accepted.
- No lucky colours, reading stats, or proverbs. These were offered and the
  owner did not choose them.
- No landscape design. Draw it in portrait like the other sleep screens.
- No new settings beyond the one new sleep screen value.

## Anti-goals (a failure even if it builds)

- A wrong lunar date or a wrong important-day date. A calendar that is
  wrong is worse than none.
- Hand-drawn, jittery, or random-looking digits.
- Big empty areas on the screen.
- Slow sleep: the screen must not add a noticeable delay before the device
  sleeps (no large files read from SD, no network).

## Current code (evidence)

- Sleep screen modes: `src/CrossPointSettings.h:20` (`SLEEP_SCREEN_MODE`,
  last value `COVER_TRANSPARENT = 8`). Menu labels:
  `src/SettingsList.cpp:12` (`SLEEP_SCREEN_VALUES`). Dispatch:
  `src/activities/boot_sleep/SleepActivity.cpp:739`.
- Date source: `ReadingLogStore::localDate()` (`src/ReadingLogStore.h:38`)
  returns false when the board has no clock or it was never set.
  `ReadingLogStore::dayNumber()` and `weekday()` already exist.
- The Calendar home theme (`src/components/CalendarHome.cpp`) already draws
  dates and a week/month grid; reuse its helpers where they fit.
- Built-in fonts: UI fonts are Ubuntu with Noto Sans Thai fallback, sizes
  10 and 12 plus an 8 pt small face (`lib/EpdFont/scripts/convert-builtin-fonts.sh:25`).
  The largest built-in face is `NOTOSANS_18_FONT_ID` (Latin only), and it is
  missing when `OMIT_FONTS` is set (`src/main.cpp:552`). None is big enough
  for the day number, which should be roughly 250 px tall on a 480×800 screen.

## Constraints

- Follow this repo's `CLAUDE.md` and `.agents/rules/` (evidence, heap
  justification, `firmware-handoff` before ready, no commit without
  approval, no push without an explicit push instruction).
- Must build on every board in `platformio.ini`. ESP32-C3 (X3/X4) is the
  resource baseline. Report flash use before and after for each board.
- Draw to the screen sizes each board reports, like the other sleep screens.
  The owner tests on hardware on the X4 Pro only; other boards get build
  checks only, and the handoff says so.
- New sleep screen value is appended after `COVER_TRANSPARENT` so stored
  settings keep their meaning.
- New user-facing strings (the menu label) go through the translation
  workflow in `.agents/rules/generated-files-cache.md`.

## Decided by the owner

- **Source of truth for the lunar calendar:** the official Thai government
  calendar for every year it has been published. For later years, use the
  traditional computed method. Mark those years in the data or generator as
  "computed, not checked against an official calendar". If the two disagree
  in a published year, the official calendar wins.
- **Zodiac year (ปีนักษัตร) changes on 1 January**, together with the B.E.
  year. It does not depend on the lunar table.
- **Second month 8 in an extra-month year (อธิกมาส)** is written
  `เดือน 8 หลัง`, e.g. `ขึ้น 4 ค่ำ เดือน 8 หลัง`. The first one stays
  `เดือน 8`.

## Calls made by the designer (owner to check)

- **Slim build:** the big-digit font and the lunar data go behind the
  existing `OMIT_FONTS` guard. On builds with `OMIT_FONTS`, the menu still
  shows the choice, but it falls back to the default sleep screen. No flash
  limit is set; report the numbers and stop if a board no longer fits.
- **Web settings page:** `src/network/html/SettingsPage.html:53` also lists
  the sleep screen values; add the new value there too.

- **Big digits:** generate a digits-only large font (glyphs `0`–`9` only)
  from a font that is already in the firmware's font sources (Ubuntu Bold,
  the UI family), using the existing `fontconvert.py`. Ten glyphs keep the
  flash cost small. This keeps the "font from the device" look. Scaling up
  an 18 pt font would look blocky, so it is not used.
- **No clock set:** if `localDate()` returns false, fall back to the default
  sleep screen.
- **Language:** the screen is always Thai, because it is a Thai calendar.
  Only the menu label is translated.
- **Week starts on Sunday**, like the Calendar home theme.
- **Important days list (default until the owner changes it):**
  - Lunar Buddhist days: มาฆบูชา, วิสาขบูชา, อาสาฬหบูชา, เข้าพรรษา,
    ออกพรรษา, ลอยกระทง.
  - Fixed days: วันขึ้นปีใหม่ (1 ม.ค.), วันเด็ก (2nd Saturday of Jan),
    วันจักรี (6 เม.ย.), วันสงกรานต์ (13 เม.ย.), วันแรงงาน (1 พ.ค.),
    วันแม่ (12 ส.ค.), วันพ่อ (5 ธ.ค.), วันรัฐธรรมนูญ (10 ธ.ค.),
    วันสิ้นปี (31 ธ.ค.).
- **Lunar data range:** supported B.E. years must cover at least 2569–2590
  (2026–2047). Outside the range, hide the lunar line, the moon and lunar
  holidays rather than show a guess; fixed days still show.

## Left to the builder

- How to compute the Thai lunar calendar (algorithm vs a compact
  precomputed table of year types / month starts, generated by a host
  script). It must handle the extra-month years (อธิกมาส, เดือน 8 สองหน)
  and extra-day years (อธิกวาร). A table is fine if it is small and its
  generator is in the repo.
- How to compute moon illumination for the drawing. An approximate
  synodic-month formula is enough for the picture, but the lunar date text
  must come from the Thai calendar, not from the moon formula.
- Exact pixel sizes and spacing, as long as the layout matches the mockup
  and fills the height on each board.
- Where the code lives (new file under `src/activities/boot_sleep/` or
  `src/components/`) and the commit split.

## Verification

- Host unit check of the lunar calendar and important days against an
  official Thai calendar (e.g. the Royal Thai government or myhora.com
  tables) for every day of at least B.E. 2569–2571, including one อธิกมาส
  year and one อธิกวาร year in the range. The mockup values (13 Nov 2026 =
  `ขึ้น 4 ค่ำ เดือน 12`, ลอยกระทง = 24 Nov 2026) are the designer's estimate
  and must be checked against the official table, not used as test truth.
- Build all boards; report flash per board before/after.
- Owner hardware test on X4 Pro: pick the new sleep screen, sleep the
  device, and compare with the mockup; also test with the clock not set
  (falls back to default screen).
