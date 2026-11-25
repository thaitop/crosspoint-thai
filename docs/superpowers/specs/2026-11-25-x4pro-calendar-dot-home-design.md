# Calendar home on the X4 Pro: match the E-Ink4-Plus design

## Intent

The owner wants the Calendar theme's home on the X4 Pro (`thai-2.0.0`) to look
like the version just accepted on the E-Ink4-Plus (`thai-eink4plus`, commits
a409f696 and a406d712; spec
`2026-11-25-calendar-day-tiles-design.md` on that branch).

## Goals

- Same day shading, unread ring, today ring and badge, selected dotted ring,
  reading dots and one-row HH:MM dot clock as the Plus. The clock's weekday
  and day badge is removed, as on the Plus (the calendar shows the date).
- Keep `CalendarHome.cpp` as close to the Plus file as the two renderers allow.

## Non-goals and anti-goals

- No layout change, no change to the 60/120 minute thresholds.
- No UI scale or gray-UI support on this branch; the X4 Pro is 1-bit at
  scale 1.0.
- No new heap allocation. No other theme or screen changes.

## Approaches

- **A. Renderer primitives (chosen by the owner).** Add `fillCircle`,
  `drawCircle` and `Color::DarkerGray` to this branch's `GfxRenderer`, without
  the scale and gray-frame paths, and draw `CalendarHome` with them as on the
  Plus.
- B. Local helpers in `CalendarHome.cpp` only. Smaller diff, but the two
  branches' calendar code would drift apart.

## Design

- `GfxRenderer`: `Color::DarkerGray = 0x0D` (75% dither, white on odd/odd
  pixels) handled in `drawPixelDither`, `fillRectDither`/`fillRectImpl` and the
  other `switch (color)` sites; `fillCircle(x, y, d, color)` and
  `drawCircle(x, y, d, lineWidth, color)` using the same span routine as the
  Plus.
- `CalendarHome.cpp`: take the Plus drawing code. Differences: thin lines are
  a fixed 2 px (no `uiScale()`), and the clock grid is computed directly in
  pixels (no `physicalRect`/`PhysicalScope`).

## Left to the builder

- Disc and dot sizes stay as the shared layout gives them on the X4 Pro panel.
- The 25/50/75% levels were tuned on the Plus glass; the owner checks them on
  the X4 Pro and may ask for changes.

## Verification

Build `x4pro`, flash, open Calendar home in a month with each reading level:
distinct levels, readable numbers, equal discs and dots, today's badge, dotted
ring on a tapped past day, one-row clock.
