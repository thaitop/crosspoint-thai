# Thai Calendar Sleep Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Thai calendar" sleep screen showing the B.E. year, zodiac, big day number, real moon phase, Thai lunar date, the current week, and the next important Thai day.

**Architecture:** All calendar rules live in a pure C++ module (`src/util/ThaiCalendar`) with no Arduino dependencies, tested on the host with gtest. Lunar year data is a small generated table (`src/util/ThaiLunarTable.h`) produced by a host Python script. The sleep screen drawing is a separate file that only lays out text and shapes; it uses a new digits-only large built-in font.

**Tech Stack:** C++20 (firmware, PlatformIO), gtest via CMake (`test/`), Python 3 (table generator, `fontconvert.py`).

**Spec:** `docs/superpowers/specs/2026-11-13-thai-calendar-sleep-screen-design.md` (read it first; the approved mockup is `.superpowers/brainstorm/16728-1794565793/content/thai-calendar-v4.html`).

## Global Constraints

- Follow `CLAUDE.md` and `.agents/rules/`. Every commit step needs the owner's explicit approval first; never push.
- All numbers on screen are Arabic digits (0–9). Screen text is always Thai.
- Second month 8 in an อธิกมาส year is written `เดือน 8 หลัง`; the first stays `เดือน 8`.
- Zodiac year changes on 1 January with the B.E. year (B.E. = Gregorian + 543).
- Lunar truth: official Thai government calendar where published, wins on conflict; later years use the traditional computed method and are marked as unchecked in the generator.
- Lunar data covers at least B.E. 2569–2590 (2026–2047). Outside the table: hide lunar line, moon, and lunar holidays; fixed days still show.
- New sleep value is appended: `THAI_CALENDAR = 9`, after `COVER_TRANSPARENT = 8`.
- No clock (`ReadingLogStore::localDate()` false) or `OMIT_FONTS` build → `renderDefaultSleepScreen()`.
- No new heap allocations in the draw path; justify any that appear (`.agents/rules/hardware-resources.md`).
- Format with `./bin/clang-format-fix -g`. Never run raw `clang-format`.
- Correction to the spec: `src/network/html/SettingsPage.html` needs no change. It reads sleep screen options from the settings API (`SettingsPage.html:177-191`); only verify the new option appears there.

## Review Focus

1. Dates in the second month 8 of an อธิกมาส year, and the day the lunar year rolls over — expect `เดือน 8 หลัง` and the correct month 1 start (Task 2 tests).
2. The first and last day covered by the table, and one day outside it — expect lunar parts hidden and fixed holidays still shown (Task 2 tests).
3. 31 December and the days after ลอยกระทง/วันพ่อ — the next important day must cross into the next year, including lunar days of the next lunar year (Task 2 tests).
4. A week that crosses a month or year boundary (e.g. Thu 1 Jan 2026) — the strip shows the previous month's days with the right numbers (Task 2 tests).
5. Boards with other screen sizes or landscape-first panels — the layout must fill the height without overlap; checked by drawing at each board's size in Task 4 and on the X4 Pro by the owner.

---

### Task 1: Lunar year table generator

**Files:**
- Create: `scripts/generate_thai_lunar_table.py`
- Create: `src/util/ThaiLunarTable.h` (generated, committed)
- Create: `test/thai_calendar/fixtures/lunar_years.csv`
- Create: `test/test_thai_lunar_table.py`
- Modify: `test/CMakeLists.txt` (add `add_test(NAME thai_lunar_table ...)` like line 101)

**Interfaces:**
- Produces (in `ThaiLunarTable.h`, namespace `thai_lunar`):
  - `enum YearType : uint8_t { NORMAL = 0, EXTRA_DAY = 1, EXTRA_MONTH = 2 };` (354 / 355 / 384 days)
  - `struct Year { int32_t month1Start; YearType type; };` where `month1Start` is the day number (as `ReadingLogStore::dayNumber`, days since 1970-01-01) of ขึ้น 1 ค่ำ เดือน 1.
  - `constexpr Year YEARS[] = {...};` sorted, consecutive (each start = previous start + previous length). First entry's lunar year contains 2026-01-01; last entry's year contains 2047-12-31.
  - `constexpr int FIRST_UNCHECKED_INDEX` — entries from this index on are computed, not checked against an official calendar.

Month rules the table relies on (used by Task 2): odd months 29 days, even months 30 days. `EXTRA_DAY` makes month 7 30 days. `EXTRA_MONTH` inserts month 8 หลัง (30 days) after month 8. Days 1–15 are ขึ้น 1–15 ค่ำ; the rest are แรม 1–14 (29-day month) or 1–15 (30-day month).

- [ ] **Step 1: Build the fixture.** In `lunar_years.csv` write one row per lunar year in the range: `be_year,month1_start_iso,type,loy_krathong_iso,source`. Take values from the official government calendar where published and from a published Thai 100-year calendar otherwise; put the source name in `source`. Stop and ask the owner to check at least 5 rows against their own calendar before continuing (this is the test truth).
- [ ] **Step 2: Write the failing test** `test/test_thai_lunar_table.py`: runs the generator to stdout and asserts, for every fixture row, the same `month1Start` and `type`, and that ขึ้น 15 ค่ำ เดือน 12 computed from the row equals `loy_krathong_iso`. Also asserts every row's start = previous start + previous length.
- [ ] **Step 3: Run it.** `python3 test/test_thai_lunar_table.py` → FAIL (script missing).
- [ ] **Step 4: Implement the generator.** Traditional Thai method (horakhun / avoman / tithi rules from the Chula Sakarat era) decides year type and month 1 start. Add an `OFFICIAL_OVERRIDES` dict keyed by B.E. year for any published year where the official calendar differs; overrides win. Emit `ThaiLunarTable.h` with a "generated by" header comment and `FIRST_UNCHECKED_INDEX` = first year after the last officially published one.
- [ ] **Step 5: Run the test** → PASS. Then `python3 scripts/generate_thai_lunar_table.py > src/util/ThaiLunarTable.h`.
- [ ] **Step 6: Commit** (owner approval): `feat: add Thai lunar year table generator`.

### Task 2: ThaiCalendar logic

**Files:**
- Create: `src/util/ThaiCalendar.h`, `src/util/ThaiCalendar.cpp`
- Create: `test/thai_calendar/CMakeLists.txt`, `test/thai_calendar/ThaiCalendarTest.cpp`
- Modify: `test/CMakeLists.txt` (`add_subdirectory(thai_calendar)`)

**Interfaces:**
- Consumes: `thai_lunar::YEARS`, `YearType` (Task 1); day numbers as `ReadingLogStore::dayNumber` / `weekday` (`src/ReadingLogStore.h:38-41`, 0 = Sunday). Copy the days-from-civil math into ThaiCalendar if linking ReadingLogStore drags in Arduino deps, and keep one implementation by making ReadingLogStore call it.
- Produces (namespace `thai_cal`):
  - `struct LunarDate { uint8_t month; bool secondEighth; bool waxing; uint8_t night; uint8_t dayInMonth; };` (`dayInMonth` 0-based)
  - `bool lunarDate(int32_t day, LunarDate& out);` false outside the table
  - `void formatLunar(const LunarDate& d, char* buf, size_t len);` → `ขึ้น 4 ค่ำ เดือน 12`, `แรม 1 ค่ำ เดือน 8 หลัง`
  - `uint16_t moonLitPermille(const LunarDate& d);` = round(1000 × (1 − cos(2π·dayInMonth/29.53)) / 2)
  - `bool moonRowSpan(int radius, int dy, uint16_t litPermille, bool waxing, int& x0, int& x1);` lit x-range (relative to centre) on row `dy`; false if the row has no lit pixels. Terminator is an ellipse with x-radius `|1 − 2k|·w` where `w` is the disc half-width on that row; waxing is lit on the right.
  - `struct ImportantDay { int32_t day; const char* name; };`
  - `bool nextImportantDay(int32_t today, ImportantDay& out);` earliest day ≥ today; searches at least 366 days ahead
  - `const char* zodiacName(int gregorianYear);` `(year − 2020) mod 12` over ชวด ฉลู ขาล เถาะ มะโรง มะเส็ง มะเมีย มะแม วอก ระกา จอ กุน
  - `const char* monthName(int tmMon); const char* weekdayName(int wday); const char* weekdayShort(int wday);` Thai strings (`อา จ อ พ พฤ ศ ส`)
- Important days (names exactly as the spec): lunar — มาฆบูชา ขึ้น 15 ค่ำ เดือน 3 (เดือน 4 in EXTRA_MONTH years), วิสาขบูชา 15 ค่ำ เดือน 6 (7), อาสาฬหบูชา 15 ค่ำ เดือน 8 (8 หลัง), เข้าพรรษา แรม 1 ค่ำ เดือน 8 (8 หลัง), ออกพรรษา 15 ค่ำ เดือน 11, ลอยกระทง 15 ค่ำ เดือน 12. Fixed — วันขึ้นปีใหม่ 1 Jan, วันเด็ก 2nd Saturday of Jan, วันจักรี 6 Apr, วันสงกรานต์ 13 Apr, วันแรงงาน 1 May, วันแม่ 12 Aug, วันพ่อ 5 Dec, วันรัฐธรรมนูญ 10 Dec, วันสิ้นปี 31 Dec.

- [ ] **Step 1: Write failing tests** in `ThaiCalendarTest.cpp` (build expected dates from `lunar_years.csv` values, not from the mockup):
  - `LunarDate.FirstDayOfTable` / `LastDayOfTable` → true; one day before / after → false.
  - `LunarDate.MonthOneStart` → month 1, waxing, night 1 for each fixture `month1_start`.
  - `LunarDate.SecondEighth` → in an EXTRA_MONTH year from the fixture, day 30 after month 8 start gives `secondEighth == true`, `formatLunar` = `ขึ้น 1 ค่ำ เดือน 8 หลัง`.
  - `LunarDate.LoyKrathong` → every fixture `loy_krathong_iso` formats as `ขึ้น 15 ค่ำ เดือน 12`.
  - `Moon.NewAndFull` → dayInMonth 0 → ≤ 10‰; dayInMonth 14 → ≥ 990‰.
  - `Moon.RowSpan` → 0‰: no span; 1000‰: span = full disc width; 500‰ waxing on row 0: x0 = 0, x1 = radius; waning mirrors it.
  - `Important.SameDay` → on ลอยกระทง date returns that day.
  - `Important.CrossesYear` → from 31 Dec returns วันสิ้นปี; from 1 Jan returns วันขึ้นปีใหม่; from 11 Dec returns วันสิ้นปี.
  - `Important.ChildrensDay` → 2027 → 9 Jan 2027.
  - `Important.OutsideTable` → a date after the table still returns fixed days only.
  - `Zodiac` → 2026 มะเมีย, 2020 ชวด, 2031 กุน.
  - `Week.CrossesMonth` → for Thu 1 Jan 2026, `weekday` and day−4…day+2 give Sunday 28 Dec 2025 … Saturday 3 Jan 2026.
- [ ] **Step 2: Run** `cmake -S test -B build/test && cmake --build build/test --target ThaiCalendarTest && build/test/thai_calendar/ThaiCalendarTest` → FAIL (not built).
- [ ] **Step 3: Implement** the interfaces in `ThaiCalendar.cpp`. No heap, no `std::string`; tables are `constexpr`.
- [ ] **Step 4: Run** → all PASS; also `ctest --test-dir build/test --output-on-failure -j` shows no new failures.
- [ ] **Step 5: Commit** (owner approval): `feat: add Thai lunar calendar and holiday rules`.

### Task 3: Digits-only large font

**Files:**
- Modify: `lib/EpdFont/scripts/fontconvert.py` (new flag `--only-additional-intervals`: skip the default interval list at line 44 and export only `--additional-intervals`)
- Modify: `lib/EpdFont/scripts/convert-builtin-fonts.sh` (generate `ubuntu_digits_bold`)
- Create: `lib/EpdFont/builtinFonts/ubuntu_digits_bold.h` (generated)
- Modify: `lib/EpdFont/builtinFonts/all.h`, `lib/EpdFont/scripts/build-font-ids.sh`, `src/fontIds.h` (`DIGITS_LARGE_FONT_ID`), `src/main.cpp:91-111` and `:552-556` (define and `insertFont` inside the `#ifndef OMIT_FONTS` blocks)

**Interfaces:**
- Produces: `DIGITS_LARGE_FONT_ID`, available only when `OMIT_FONTS` is not defined; contains only U+0030–U+0039, Ubuntu Bold, 1-bit, uncompressed (drawn straight from flash, no decompression buffer).

- [ ] **Step 1:** Add the flag; run `python fontconvert.py ubuntu_digits_bold <size> ../builtinFonts/source/Ubuntu/Ubuntu-Bold.ttf --only-additional-intervals --additional-intervals 0x30,0x39`. Pick `<size>` so the generated glyph height of `0` is 240–260 px (start near 170; ppem = size × 150 / 72). Check the glyph table in the header for exactly 10 glyphs.
- [ ] **Step 2:** Register the font (both `OMIT_FONTS` blocks), add its ID to `build-font-ids.sh`, regenerate `src/fontIds.h`. If `EpdFontFamily` needs all four styles, pass the same bold font or nullptr as the existing constructor allows.
- [ ] **Step 3:** `pio run -e <X4 Pro env>` and the slim env build; record flash use before/after in the task log. Expected: builds pass; flash growth roughly the header's bitmap size.
- [ ] **Step 4: Commit** (owner approval): `feat: add large digits font for calendar screen`.

### Task 4: Sleep screen

**Files:**
- Modify: `src/CrossPointSettings.h:20-30` (`THAI_CALENDAR = 9` before `SLEEP_SCREEN_MODE_COUNT`)
- Modify: `src/SettingsList.cpp:12-23` (`values[CrossPointSettings::THAI_CALENDAR] = StrId::STR_THAI_CALENDAR;`)
- Modify: `lib/I18n/translations/english.yaml` (`STR_THAI_CALENDAR: "Thai calendar"`), `thai.yaml` (`"ปฏิทินไทย"`); run `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`
- Create: `src/activities/boot_sleep/ThaiCalendarSleepScreen.h/.cpp`
- Modify: `src/activities/boot_sleep/SleepActivity.cpp:739-753` (new case), `SleepActivity.h` (`renderThaiCalendarSleepScreen()`)

**Interfaces:**
- Consumes: everything in Task 2; `DIGITS_LARGE_FONT_ID` (Task 3); `ReadingLogStore::localDate`, `dayNumber`.
- Produces: `void drawThaiCalendarSleepScreen(const GfxRenderer& renderer, const struct tm& date);` — draws to the current buffer, no display call.

- [ ] **Step 1:** Add the enum value, label, translation; regenerate i18n.
- [ ] **Step 2:** In `SleepActivity`, `case THAI_CALENDAR: return renderThaiCalendarSleepScreen();`. That method: under `OMIT_FONTS`, or when `localDate` fails → `renderDefaultSleepScreen()`; else `clearScreen()`, `drawThaiCalendarSleepScreen`, `displayBuffer(kSleepClean)`. No `invertScreen` (paper background, black ink).
- [ ] **Step 3:** Implement `drawThaiCalendarSleepScreen` following the v4 mockup, top to bottom: header (`พ.ศ. 2569 · ปีมะเมีย`, `พฤศจิกายน  วันศุกร์`, UI_12/UI_10), day number (`DIGITS_LARGE_FONT_ID`), moon disc (fillRect per row from `moonRowSpan`, outline with `drawArc`/circle points; radius ≈ 64 px on 480 wide), lunar line (UI_12 bold), week strip (7 boxes Sunday–Saturday, today filled black with white text), rule + `วันสำคัญที่กำลังมาถึง` + name (bold) + `24 พ.ย. · อีก 11 วัน` or `วันนี้` when day == today. Measure every section's height, then split the leftover height into equal gaps so the content spans top margin to bottom margin. Outside the table: skip moon and lunar line; the gaps grow.
- [ ] **Step 4:** Draw at each board's screen size: build every env in `platformio.ini` (`pio run -e <env>`, full output to a task log). Report flash per board before/after.
- [ ] **Step 5:** Check the web settings page lists "Thai calendar" (open `/settings` on device or read the API JSON in a host test if one exists).
- [ ] **Step 6:** `./bin/clang-format-fix -g`; run the `firmware-handoff` skill (checks, four reviews, owner acceptance).
- [ ] **Step 7: Owner hardware test on X4 Pro:** choose Thai calendar, sleep, compare with the v4 mockup; check the lunar date against a wall calendar; set the clock unset/invalid and confirm the default sleep screen appears.
- [ ] **Step 8: Commit** (owner approval): `feat: add Thai calendar sleep screen`.
