# Shelf home theme — design

Size: one new theme-drawn home (small-to-medium change).

## Intent

The owner likes the look of the Hti Cross home screen (BOOX Picco) and wants a
similar home theme on every device, with one feature of our own. The new theme
is called "Shelf" ("ชั้นหนังสือ" in Thai).

## Goals

- A new home theme: a large "continue reading" card, a "recent" row of covers,
  and a rounded bottom bar of tools.
- Works on every device profile: button-only (X4) and touch (Picco and others),
  with or without PSRAM, with or without a clock.
- Our own feature: a reading-time line inside the continue-reading card.

## Non-goals

- No book store screen, no sync button, no "more" (⋮) menu.
- No change to the Library, file browser, settings or other screens. They keep
  the Lyra style, like the Ledger, Today and Calendar themes.
- No Thai date line, no Thai ornaments (offered and declined).
- No Hytexts name, logo or branding.

## Anti-goals

- More memory or slower home loading on the ESP32-C3 than the existing homes.
- A layout that breaks or overlaps on small screens.

## Approaches

1. **New `ShelfHome` class beside `TodayHome` and `CalendarHome` (chosen).**
   Plugs into the existing theme-home switch in `HomeActivity`, reuses the
   `homeui` helpers and the shared tool list.
2. Add a second mode to `TodayHome`: mixes two layouts in one class; harder to
   maintain.
3. Build with SDK UI components like Cover Grid: needs PSRAM, so not every
   device can use it.

## Design

- Settings: add `SHELF = 8` to `CrossPointSettings::UI_THEME` (stored values do
  not change), add it to `homeThemeOptions()`, add `UITheme::hasShelfHome()`,
  and map it to `LyraTheme` in `UITheme::setTheme()`. New strings
  `STR_THEME_SHELF` (EN "Shelf", TH "ชั้นหนังสือ") through the translation
  workflow.
- `src/components/ShelfHome.{h,cpp}`, same interface as `TodayHome`:
  `loadProgress()`, `render()`, `hitTest()`. Flat selection indices: current
  book, recent books, then tools.
- Layout, top to bottom:
  - Status line from the theme (clock where available, battery).
  - Continue-reading card with rounded border: cover, title, author, percent
    and progress bar (no chapter "X of Y": saved progress holds only a percent), reading-time line, and a black capsule
    resume button; a selected card gets a thicker border.
  - Reading-time line: "วันนี้อ่าน N นาที · ติดต่อกัน N วัน" from
    `ReadingLogStore` (`secondsOn`, `streak`). Hidden when the device has no
    local date (X4 without RTC) or when both values are zero.
  - "Recent" heading and up to 3 more covers with title and percent bar. No
    paging and no page dots. At most 4 covers in total on screen. When the
    screen is too narrow for 3 covers, show 2.
  - Bottom capsule bar with the five existing tools (Files, Library,
    Plugins/OPDS, Transfer, Settings). The selected tool is a black capsule with
    icon and label; the others show only their icon.
- Empty state (no recent books): card shows a short "no book yet" message; the
  tool bar still works.
- Input: button navigation moves through the flat indices; touch uses
  `hitTest()`. Button hints as in the other theme homes.

## Left to the builder

- Exact sizes, radii and fonts per screen size, within the metrics used by the
  other homes.
- Whether "chapter X of Y" is available cheaply from the existing progress
  load; if not, show only the percent.
- Strings for the reading-time line and empty state.

## Verification

- Build every device env.
- `scripts/sim_check.py` screenshots of the Shelf home on all 9 profiles
  (update its per-device key/tap tables if the home menu order changes).
- Hardware testing by the owner before any PR.

## Revision 2 (2026-11-27, after the owner's hardware test)

The owner found the area below the recent row too empty and asked for a
scrolling recent row, bigger recent covers, rounded covers, and a GitHub-style
reading-activity grid. Agreed layout, top to bottom:

- Card: featured cover 140 px tall (was 168), so the card is about 28 px
  shorter. Contents unchanged; the title gets the lines that fit above the
  progress and reading-time rows.
- Recent row: up to 9 books, 3 per page (2 on narrow screens), with page dots
  under the row. Touch: swipe left/right turns the page. Buttons: moving the
  selection past the last cover of a page shows the next page. Cover height
  fills the space left over, capped by the tile width (2:3 covers).
- All covers on this home have rounded corners.
- Activity grid: last 13 weeks (3 months), 7 rows (Sun to Sat), one column per
  week, the right column is the current week; 12 px cells, left-aligned.
  Levels as in the Calendar theme: none (outline), under 60 min (light gray),
  under 120 min (dark gray), 120 min or more (black). To its right: "Last 3
  months", total time, and number of reading days. Without a clock, the clock
  hint from the Today theme replaces the grid.
- Data: the reading log keeps minutes per day for the last 91 days (1 byte per
  day, capped at 255) in `reading_log.json` as `history` + `historyEnd`.
  Existing files are backfilled from the week and month logs on load; older
  days start empty. Older firmware ignores the new keys.
- Expected cover size: about 185 px on Picco, about 145 px on X4/X4 Pro (button
  hints take 40 px).
- Cost: entering Home reads progress for up to 10 books (like Ledger) and the
  first visit generates thumbs at the new heights.
