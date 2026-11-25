# X4 Pro Calendar Dot Home Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the X4 Pro Calendar home look like the accepted E-Ink4-Plus version.

**Architecture:** Add a 75% dither color and circle primitives to this branch's `GfxRenderer` (no UI scale, no gray UI), then port the Plus drawing code in `CalendarHome.cpp` onto them.

**Tech Stack:** C++ / GfxRenderer, PlatformIO env `x4pro` (`~/.platformio` core 6.1.19).

**Spec:** `docs/superpowers/specs/2026-11-25-x4pro-calendar-dot-home-design.md`

**Reference code:** `git show a406d712:src/components/CalendarHome.cpp` and
`git show a406d712 -- lib/GfxRenderer/` (run in `../crosspoint-thai-eink4plus`).

## Global Constraints

- No layout change; `DARK_MINUTES` 60, `BLACK_MINUTES` 120 stay.
- No heap allocation; no other theme or screen changes.
- `Color::DarkerGray = 0x0D`, 75% dither: black unless x and y are both odd.
- Thin line = 2 px. Today ring = `disc + 8` diameter, 2 px. Badge = `max(8, disc / 4)` black on a `badge + 4` white disc.

## Review Focus

- `-Wswitch` on any `switch (color)` that misses `DarkerGray`: the build must have no new warnings.
- Clock with no time set (`date == nullptr`): four dashes, no colon.
- 12-hour setting: hour shown as on the Plus (code above the glyph loop is unchanged).
- Month with 6 rows: discs still fit (`disc < 16` early return unchanged).
- Day 1 being today: badge must not draw outside the screen at the top row.

---

### Task 1: Renderer `DarkerGray`, `fillCircle`, `drawCircle`

**Files:**
- Modify: `lib/GfxRenderer/GfxRenderer.h:33` (enum), near `:320` (declarations)
- Modify: `lib/GfxRenderer/GfxRenderer.cpp:1052-1095` (dither), `:1100+` (`fillRectImpl` pattern), `:1290` (`maskRoundedRectOutsideCorners`), `:1378` (`fillRoundedRect` switch), template instantiations

**Interfaces:**
- Produces: `Color::DarkerGray`; `void fillCircle(int x, int y, int diameter, Color color) const;`
  `void drawCircle(int x, int y, int diameter, int lineWidth, Color color) const;` — circle inscribed in the square at (x, y), outline drawn inward; `lineWidth >= diameter / 2` gives a disc.

- [ ] **Step 1:** Add `DarkerGray` to the enum and to every place that handles `DarkGray` (`drawPixelDither` specialization, `fillRectDither`, `fillRectImpl` pattern branch, corner mask, `fillRoundedRect` switch, explicit instantiation). Skip the gray-frame/`grayBlend` parts of the Plus diff — they do not exist here.
- [ ] **Step 2:** Copy `forEachCircleSpan` (anonymous namespace) and `fillCircle`/`drawCircle` from a406d712, without the `uiScaleActive_` block. Add `#include <cmath>` if missing.
- [ ] **Step 3:** Build: `pio run -e x4pro 2>&1 | tail -20`. Expected: SUCCESS, no new warnings in GfxRenderer.

### Task 2: Port CalendarHome drawing

**Files:**
- Modify: `src/components/CalendarHome.cpp`

**Interfaces:**
- Consumes: Task 1 `fillCircle`, `drawCircle`, `Color::DarkerGray`.

- [ ] **Step 1:** Apply the `thai-2.0.0..a406d712` diff of this file (shown by `git diff thai-2.0.0 a406d712 -- src/components/CalendarHome.cpp`), with two changes:
  - `thinLine()` is dropped; call sites pass `2`.
  - In `drawClock`, delete `physicalRect(...)` and the `PhysicalScope`; compute `cell`, `x`, `y`, `d` from the padded rect directly.
  Remove `NUMBER_FONT_ID`; keep the includes (`I18n.h` is still used by the month header).
- [ ] **Step 2:** Build: `pio run -e x4pro 2>&1 | tail -20`. Expected: SUCCESS, no new warnings.
- [ ] **Step 3:** Run `clang-format -i` on the touched files (repo `.clang-format`) and rebuild.
- [ ] **Step 4:** Handoff: run `firmware-handoff`. Owner flashes the X4 Pro (SD-card flash) and checks: distinct 25/50/black levels, 75% ring on unread past days, today ring + badge, dotted ring on a tapped past day, empty dots as rings, one-row HH:MM clock. Commit only on the owner's approval.
