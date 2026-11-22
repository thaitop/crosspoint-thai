# Heavier UI text on Metalio E-Ink4-Plus — design (short)

## Intent

On the Plus, UI text (menus, lists, headers) is too thin; some Thai and Latin
strokes look almost broken. The owner wants UI text that reads as solid as on
the X4 Pro. Book pages are not part of this change.

## Why it is thin today

- Boards with `uiScale` != 1 draw the UI from the 10/12/8 pt Ubuntu 2-bit
  faces, scaled 1.5x at runtime (`lib/GfxRenderer/ScaledFont.cpp`).
- The Plus draws menus as gray frames (`CROSSPOINT_GRAY_UI` is on for this
  board, `lib/hal/HalDisplay.h`). Glyph level 1/2 shows as light/dark edge
  gray; only level 3 is black. With the current mapping a pixel is black only
  above ~77% coverage, so stems keep a ~1 px black core.
- Ubuntu Regular / Noto Sans Thai Regular at ~18 px on a ~300 ppi panel has
  ~1.5–2 px stems, so even sharper rasterizing would stay thin.

## Goals / non-goals / anti-goals

- Goal: regular-weight UI text on scaled-UI boards looks like the tested
  spike `firmware_medium_dark_b`, then lightened to the 30/40/55% level map
  (owner approved the final build on the device).
- Non-goal: X4 / X4 Pro look (they use the 1-bit faces, unchanged); reader
  fonts; Hebrew/Arabic weight (no Medium sources in the repo).
- Anti-goal: counters (`e a 8`, Thai loop heads) filling in.

## Approaches considered

| | Approach | Result |
| --- | --- | --- |
| A | Lower `INK_FROM` (30% -> 15–25%) | Tried: no visible change, the extra pixels only become light gray in a gray frame. |
| A2 | Remap coverage levels darker | Tried: slightly better, still thin. |
| **1 (chosen)** | Medium weight for the regular 2-bit UI faces **plus** the darker level map | Tried on device: owner said it looks right. |
| 2 | Synthetic bold in `ScaledFont` | Not tried; counters fill in, less faithful than a real Medium face. |
| B | Native-size UI fonts, no runtime scaling | Sharper edges but same thin stems; bigger generator change and diverges from upstream `ScaledFont`. |

## Design

1. **Font sources.** Add `Ubuntu-Medium.ttf` (Ubuntu Font Licence, from
   google/fonts) and `NotoSansThai-Medium.ttf` (OFL, from notofonts) under
   `lib/EpdFont/builtinFonts/source/`.
2. **Generator.** In `convert-builtin-fonts.sh`, the 2-bit loop uses Ubuntu
   Medium and Noto Sans Thai Medium for the Regular entries (8, 10, 12). Noto
   Sans Latin fallback, Hebrew, Arabic and the Vietnamese Ubuntu cut stay
   Regular. Bold 2-bit faces and all 1-bit faces are unchanged.
3. **Kerning limit.** Ubuntu Medium has more kerning pairs (Greek, Cyrillic,
   …) than the 255-class format holds, so `fontconvert.py` fails. Add an
   option `--kern-intervals` that keeps only pairs whose two code points fall
   in the given intervals, and use it for these three faces with Latin
   (U+0000–U+024F), General Punctuation (U+2000–U+206F) and Thai
   (U+0E00–U+0E7F). Other scripts in these faces draw without kerning.
4. **Level map.** In `ScaledFont::scaleGlyph`, replace the linear `INK_FROM`
   mapping with fixed thresholds: level 1 from 30% coverage, level 2 from 40%,
   level 3 (black) from 55%. (25/35/45% was tried in the full build and was
   too heavy everywhere.) Update the comment. This applies to every scaled
   UI face, bold included (tested that way).
5. Regenerate only the three changed headers; build `metalio_eink4_plus`.

## Constraints

- Flash: spike used 92.0% of the 6.25 MB app slot (+~4 KB).
- `thai-eink4plus` branch only; `thai-2.0.0` (X4 Pro) is not touched.

## Left to the builder

- Exact option name and help text for the kerning filter.
- Verification: a header regenerated with the Regular sources and without the
  new option must stay byte-identical (confirmed for `ubuntu_8_regular_2bit.h`
  in the spike); build the X4 env too, to show its 1-bit faces are unchanged.
- Owner re-checks on the Plus: settings, file browser, a popup, with Thai and
  English text, and confirms Thai mark stacking still looks right.
