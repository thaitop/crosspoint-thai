# Calendar home: day discs and dot clock for the E-Ink4-Plus

## Intent

On the Metalio E-Ink4-Plus (1216x684, UI scale 1.5) the Calendar theme's home
looked coarse: day discs, the reading dots and the clock digits were laid out
in UI units and scaled, so edges stepped and same-size shapes came out 1 px
apart. Levels were pale and hard to tell apart. The owner also wanted a clearer
"today" and a one-row clock.

## Goals

- Every pattern and shape drawn in panel pixels, laid out once per group so
  equal shapes are equal.
- Reading levels that read as one rule: the longer you read, the darker.
- One visual language of round dots; a today marker stronger than a ring.

## Non-goals and anti-goals

- No true gray: the full-tone gray table makes every gray area flash through
  black (`lib/GfxRenderer/GfxRenderer.cpp`, comment above `kGrayUiLevels`).
- No renderer or SDK change; no change to the 60/120 minute thresholds.
- No new heap allocation.

## Approaches

- **A. Better 1-bit dither (chosen).** Panel-pixel patterns. No flash risk.
- B. True 16-level gray. Smooth, but the gray table is global; using it only
  here needs per-frame table switching in the SDK driver.
- C. The two edge gray levels (9, 12) over areas. No flash, but the code notes
  they are grainy over an area.

## Design (as accepted on hardware)

| Day | Disc | Number |
|---|---|---|
| future | none | black |
| past, no counted reading | 2 px ring in 75% dots | black |
| 1–59 min | 25% | black, bold |
| 60–119 min | 50% | black, bold |
| 120+ min | black | white |
| today | outer ring, 2 UI units (3 px at scale 1.5), + black badge on its top right, ringed in white | – |
| selected | ring of small dots every 15° | – |

Tried and rejected on the device: 50%/75% fills (dot gain makes them look
alike), white numbers on 50% (break up), a chip behind numbers (confusing),
no frame on unread days, a 1 px frame (breaks), a date badge in the clock,
rounded-square day tiles (discs match the clock and reading dots better).

- Day grid: converted to panel pixels once; every disc has one size and is
  placed from one origin. Numbers stay in UI units.
- Reading dots: empty dots are 2 px rings, read dots solid; spread to the
  tile padding, laid out once in panel pixels.
- Clock: HH:MM on one row of round white dots (dot-matrix 5x7 glyphs plus a
  colon), square cells sized to fit the padded tile, centered, laid out once
  in panel pixels. The date badge was removed (the calendar shows the date).

All in `src/components/CalendarHome.cpp`. Discs, rings and dots share one
row-span helper.

## Verification

Build `metalio_eink4_plus`, flash, open Calendar home in a month with every
level: even shading, distinct levels, readable numbers, equal tiles and dots,
today's badge, tapping a past day shows its time with a dotted ring.
