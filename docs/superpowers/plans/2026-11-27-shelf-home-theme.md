# Shelf Home Theme Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Shelf" ("ชั้นหนังสือ") home theme: a large continue-reading card with a reading-time line, a row of recent covers, and a capsule tool bar, on every device.

**Architecture:** A new theme-drawn home class `ShelfHome`, a sibling of `TodayHome` and `CalendarHome`, selected by a new `UI_THEME` value `SHELF = 8`. Other screens use `LyraTheme`, as for Ledger, Today and Calendar. Pure layout decisions live in a header-only `ShelfLayout.h` so host tests can check them.

**Tech Stack:** C++20, PlatformIO/ESP-IDF firmware, GfxRenderer, GoogleTest host tests (CMake under `test/`), YAML i18n with `scripts/gen_i18n.py`.

**Spec:** `docs/superpowers/specs/2026-11-27-shelf-home-theme-design.md`

## Global Constraints

- Stored theme values stay the same; new value `SHELF = 8`, `UI_THEME_COUNT` moves to 9.
- Theme name: EN "Shelf", TH "ชั้นหนังสือ".
- At most 4 covers on the home screen (1 featured + 3 recent); 2 recent when 3 do not fit. No paging and no page dots.
- Reading-time line format (TH): "วันนี้อ่าน N นาที · ติดต่อกัน N วัน". Hidden when there is no local date, or when today's minutes and the streak are both 0.
- Chapter "X of Y" is **not** shown: `loadBookProgress()` (`src/util/BookProgress.cpp`) returns only a percent, and the spec allows percent only.
- No store screen, sync button, "⋮" menu, Thai date line, Thai ornaments, or Hytexts name/logo.
- Allocation: `ShelfHome` is one `makeUniqueNoThrow` per home entry, like the other homes; no other new heap use. Covers are read from SD thumbs at draw time, not cached in RAM.
- Every user-facing string goes through `tr(STR_*)`; add keys to `english.yaml` and `thai.yaml`, then run `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.
- Format with `./bin/clang-format-fix -g`. No commit without the owner's approval; the firmware-handoff skill runs before the work is called ready.

## Review Focus

1. No recent books: the card shows `STR_NO_OPEN_BOOK` / `STR_HOME_EMPTY_HINT`, and tool indices start at 0, so buttons and taps still open the tools.
2. Only 1 or 2 recent books: the recent row shows only the books there are, and the "Recent" heading is hidden when there are none past the first.
3. Narrow screens and landscape: the recent row drops to 2 covers; nothing overlaps the tool bar. Check portrait and landscape in the simulator.
4. No clock (original X4): the reading-time line is hidden, and the card does not leave an empty gap.
5. Missing cover thumbs or unknown progress (-1): the placeholder cover is drawn and the progress bar is hidden.

Tests for 2, 3 and 4 are in Task 1 (`ShelfLayoutTest`). Items 1 and 5 are covered by the simulator check in Task 4.

---

### Task 1: Pure layout rules (`ShelfLayout.h`) with host tests

**Files:**
- Create: `src/components/ShelfLayout.h`
- Create: `test/shelf_layout/CMakeLists.txt`, `test/shelf_layout/ShelfLayoutTest.cpp`
- Modify: `test/CMakeLists.txt` (add `add_subdirectory(shelf_layout)` beside `home_button`, line 72)

**Interfaces:**
- Produces (namespace `shelf`, all `constexpr`, no includes beyond `<algorithm>`):
  - `constexpr int MAX_RECENT = 3;`
  - `constexpr int MIN_TILE_WIDTH = 110;` and `constexpr int TILE_GAP = 16;`
  - `constexpr int recentColumns(int width)` → 3 when `3 * MIN_TILE_WIDTH + 2 * TILE_GAP <= width`, else 2.
  - `constexpr int shownRecent(int bookCount, int width)` → `std::clamp(bookCount - 1, 0, recentColumns(width))`.
  - `constexpr bool showReadingLine(bool hasDate, int minutesToday, int streakDays)` → `hasDate && (minutesToday > 0 || streakDays > 0)`.

- [ ] **Step 1: Write the failing tests** in `ShelfLayoutTest.cpp`:

```cpp
#include "components/ShelfLayout.h"
#include <gtest/gtest.h>

TEST(ShelfLayout, ThreeColumnsWhenWide) { EXPECT_EQ(shelf::recentColumns(362), 3); EXPECT_EQ(shelf::recentColumns(440), 3); }
TEST(ShelfLayout, TwoColumnsWhenNarrow) { EXPECT_EQ(shelf::recentColumns(361), 2); EXPECT_EQ(shelf::recentColumns(250), 2); }
TEST(ShelfLayout, ShownRecentNeverExceedsBooks) {
  EXPECT_EQ(shelf::shownRecent(0, 440), 0);
  EXPECT_EQ(shelf::shownRecent(1, 440), 0);
  EXPECT_EQ(shelf::shownRecent(3, 440), 2);
  EXPECT_EQ(shelf::shownRecent(10, 440), 3);
  EXPECT_EQ(shelf::shownRecent(10, 300), 2);
}
TEST(ShelfLayout, ReadingLineNeedsDateAndActivity) {
  EXPECT_FALSE(shelf::showReadingLine(false, 25, 4));
  EXPECT_FALSE(shelf::showReadingLine(true, 0, 0));
  EXPECT_TRUE(shelf::showReadingLine(true, 0, 3));
  EXPECT_TRUE(shelf::showReadingLine(true, 12, 0));
}
```

`CMakeLists.txt` follows `test/home_button/CMakeLists.txt`: one executable `ShelfLayoutTest`, include `${REPO_ROOT}/src`, link `crosspoint_test_common GTest::gtest_main`, `gtest_discover_tests`.

- [ ] **Step 2: Run, expect a compile failure** (header missing):
`cmake -S test -B build/test && cmake --build build/test --target ShelfLayoutTest`

- [ ] **Step 3: Create `src/components/ShelfLayout.h`** with the interface above.

- [ ] **Step 4: Run, expect PASS:**
`cmake --build build/test --target ShelfLayoutTest && build/test/shelf_layout/ShelfLayoutTest`
Then `ctest --test-dir build/test --output-on-failure -j` shows no new failures.

- [ ] **Step 5: Commit (only after owner approval):** `feat(home): add Shelf layout rules`

### Task 2: Theme setting, strings and theme mapping

**Files:**
- Modify: `src/CrossPointSettings.h:207-217` (add `SHELF = 8` before `UI_THEME_COUNT`)
- Modify: `src/SettingsList.h:278-286` (push `{StrId::STR_THEME_SHELF, CrossPointSettings::SHELF}` after Calendar)
- Modify: `src/components/UITheme.h`, `src/components/UITheme.cpp:41-58` (add `static bool hasShelfHome()`; add `case SHELF:` to the Lyra group and its comment)
- Modify: `lib/I18n/translations/english.yaml`, `lib/I18n/translations/thai.yaml` (next to `STR_THEME_CALENDAR`), then regenerate.

**Interfaces:**
- Produces: `CrossPointSettings::SHELF`, `UITheme::hasShelfHome()` (true when `SETTINGS.uiTheme == SHELF`), and these strings:

| Key | english | thai |
| --- | --- | --- |
| `STR_THEME_SHELF` | `Shelf` | `ชั้นหนังสือ` |
| `STR_SHELF_RECENT` | `Recent` | `อ่านล่าสุด` |
| `STR_SHELF_READ_TODAY` | `Read today` | `วันนี้อ่าน` |
| `STR_SHELF_STREAK` | `streak` | `ติดต่อกัน` |

- [ ] **Step 1: Make the edits above and regenerate i18n** with `python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/`.
- [ ] **Step 2: Build one env, expect success:** `pio run -e x4pro` (log to a task file). The theme appears in Settings and falls back to the standard Lyra home until Task 3.
- [ ] **Step 3: Commit (only after owner approval):** `feat(settings): add Shelf home theme option`

### Task 3: `ShelfHome` drawing and `HomeActivity` wiring

**Files:**
- Create: `src/components/ShelfHome.h`, `src/components/ShelfHome.cpp`
- Modify: `src/activities/home/HomeActivity.h:19-20,89` (add `std::unique_ptr<ShelfHome> shelfHome;`, include it in `hasThemeHome()`)
- Modify: `src/activities/home/HomeActivity.cpp` at each theme-home switch: create (`:240-258`), reset (`:279-280`), tap hit test (`:402-405`), cover load (`:622-639`), render (`:653-658`)

**Interfaces:**
- Consumes: `shelf::shownRecent`, `shelf::showReadingLine`, `shelf::TILE_GAP` (Task 1); `STR_SHELF_*` (Task 2); `homeui::drawCover`, `homeui::drawProgressTrack`, `homeui::formatDuration`, `homeui::Tool` (`src/components/HomeWidgets.h`); `READING_LOG.secondsOn/streak`, `ReadingLogStore::localDate/dayNumber`; `loadBookProgress()`.
- Produces:

```cpp
class ShelfHome {
 public:
  static constexpr int MAX_BOOKS = 1 + shelf::MAX_RECENT;  // 4
  static constexpr int MAX_TOOLS = 5;
  static constexpr int FEATURED_COVER_HEIGHT = 168;
  static constexpr int RECENT_COVER_HEIGHT = 150;
  using Tool = homeui::Tool;
  static int coverHeightFor(size_t index);  // 0 → featured, else recent
  void loadProgress(const std::vector<RecentBook>& books);  // percent per book, -1 unknown
  void render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools, int toolCount,
              int selected);
  int hitTest(int x, int y) const;
 private:
  std::array<int8_t, MAX_BOOKS> progress;  // filled with -1
  std::array<Rect, MAX_BOOKS + MAX_TOOLS> targets;
  int targetCount = 0;
};
```

Selection indices are flat: loaded books first (0 = featured, then recent books in order), then tools. `HomeActivity` loads `ShelfHome::MAX_BOOKS` recent books, so `recentBooks.size()` is at most 4 and indices match the existing `activateSelection()` with no change. Books past `shownRecent()` (narrow screen) get no target and are skipped by `render`; `HomeActivity` must not load them: pass `1 + shelf::shownRecent(MAX_BOOKS, safeWidth)` to `loadRecentBooks()` where `safeWidth` is the content width (`getScreenSafeArea(...).width - 2 * metrics.contentSidePadding`).

Drawing, top to bottom, inside `getScreenSafeArea(renderer, true)` with `metrics.contentSidePadding`:
1. `GUI.drawHeader(...)` status line, as in `TodayHome::render`.
2. Card: `drawRoundedRect` border (2 px, radius 18). Inside: cover at `FEATURED_COVER_HEIGHT` on the left; title (bold `UI_12`, up to 2 lines), author (`UI_10`, 1 line), percent text right-aligned and `drawProgressTrack` (hidden when progress is -1); reading-time line in `UI_10` when `showReadingLine`, text `"<READ_TODAY> <formatDuration(min)> · <STREAK> <n> <STR_UNIT_DAYS>"`; under the cover, a full-card-width capsule (`fillRoundedRect`, radius = height/2, height 44) with bold `STR_CONTINUE_READING` on the left and the truncated title on the right with ">". The capsule is black always; when the card is selected, the card border is 4 px instead of 2 px.
   Empty state (no books): card holds `STR_NO_OPEN_BOOK` (bold) and `STR_HOME_EMPTY_HINT`, centered, no capsule, no target.
3. `STR_SHELF_RECENT` heading (bold `UI_12`) when `shownRecent > 0`, then `shownRecent` equal tiles separated by `TILE_GAP`: cover at `RECENT_COVER_HEIGHT`, one-line truncated title, short progress track + percent. A selected tile gets a 3 px rounded outline.
4. Capsule tool bar, height 56, fixed at the bottom of the safe area minus `metrics.buttonHintsHeight`: an outlined capsule across the content width; each tool gets an equal cell with its 32 px icon centered; the selected tool's cell becomes a black capsule with white icon and bold `UI_10` label. Tool icons come from the same icon set as `homeui::drawToolBar` (move the private `toolIcon()` in `HomeWidgets.cpp` into `homeui` as `const uint8_t* toolIcon(UIIcon)` rather than copying it).
5. In landscape the card takes the left half and the recent row the right half (stacked tiles still use `shownRecent` against the half width); the tool bar stays at the bottom.

- [ ] **Step 1: Write `ShelfHome.h/.cpp`** to the interface and drawing order above; `hitTest` is the same loop as `TodayHome::hitTest`.
- [ ] **Step 2: Wire `HomeActivity`** at each listed switch, following the `todayHome` branch beside it.
- [ ] **Step 3: Format and build:** `./bin/clang-format-fix -g`, then `pio run -e x4pro` and `pio run -e picco`. Expect success. Record RAM/flash use from each build summary next to the Task 2 numbers in the task log.
- [ ] **Step 4: Commit (only after owner approval):** `feat(home): add Shelf home`

### Task 4: Simulator check on all 9 devices

**Files:**
- Modify: `scripts/sim_check.py:122-132` — `seed_sd()` also writes `uiTheme` when the environment variable `SIM_CHECK_UI_THEME` is set (int).

- [ ] **Step 1: Add the override** (`settings["uiTheme"] = int(os.environ["SIM_CHECK_UI_THEME"])` when set).
- [ ] **Step 2: Run** `SIM_CHECK_UI_THEME=8 uv run -p 3.12 scripts/sim_check.py`. Expected: every device passes, and the gallery `build/sim-check/<time>/index.html` shows the Shelf home. Check by eye: card, reading line (or none), recent row, capsule bar, no overlap; one device in landscape.
- [ ] **Step 3: Check the empty state** by running one button device and one touch device with no book on the seeded SD (temporarily point the script at an empty `books/`), and confirm the tools still open.
- [ ] **Step 4: Build every release env** (`default`, `x4pro`, `x4c`, `metalio_eink4`, `papermono`, `eego_a4`, `picco`, `sticky`, `slim`). Expect success for all.
- [ ] **Step 5: Run firmware-handoff** (`.agents/skills/firmware-handoff/SKILL.md`), then hand over for owner hardware testing in all four orientations.

---

## Revision 2 tasks (spec "Revision 2")

### Task 5: Reading history ring (`src/util/ReadingHistory.h`) with host tests
- Header-only `class ReadingHistory { static constexpr int DAYS = 91; void set(int32_t day, uint32_t seconds); int minutesOn(int32_t day) const; int32_t endDay() const; ... }`.
- `set`: minutes = seconds >= 60 ? min(255, (seconds + 30) / 60) : 0; a day after `endDay()` advances the end and clears skipped days; a day within the last 91 overwrites; older days are ignored.
- Serialization helpers: `uint8_t at(int i) const` / `void load(int32_t end, const uint8_t* minutes, int count)` in ring order (index = day % DAYS).
- Tests (`test/reading_history/`): empty → 0; set + read; rounding and 60 s threshold; 255 cap; advancing clears skipped days; gap ≥ 91 clears all; day older than window ignored; load/at round-trip.

### Task 6: Wire history into `ReadingLogStore`
- Member `ReadingHistory history;`, JSON keys `history` (array of 91 ints, ring order) and `historyEnd`.
- `fromJson` without `history`: backfill from month log (by monthKey) then week log.
- `addReading`: `history.set(today, days[0].seconds)` after the week log update. Accessor `int historyMinutes(int32_t day) const`.

### Task 7: Shelf layout rules update (`ShelfLayout.h`) + tests
- `MAX_RECENT = 9`; `recentColumns(width)` unchanged; `recentPages(recentCount, columns)`; `heatLevel(minutes)` → 0/1/2/3 at 1/60/120; `HEAT_WEEKS = 13`, `HEAT_CELL = 12`, `HEAT_GAP = 3`.

### Task 8: ShelfHome redraw
- Featured cover 140; recent cover height from leftover space (capped by tile width, min 60); `coverHeightFor(renderer, index)`; page dots; rounded covers via `homeui::drawCover(..., radius)`; activity grid + stats; `turnPage(dir)` used by HomeActivity on Left/Right swipes; `maxBooks` = 1 + 9 when the row fits.
- New strings: `STR_SHELF_LAST_3_MONTHS`, `STR_SHELF_TOTAL`, `STR_SHELF_READ_DAYS`.
- Drop the landscape branch: home screens always render in portrait.

### Task 9: Verify
- Host tests, all 9 env builds, simulator 9 devices + manual 10-book run, handoff reviews.
