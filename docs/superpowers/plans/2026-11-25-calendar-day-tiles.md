# Calendar Day Tiles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Calendar home's day discs with rounded-square tiles shaded by fine panel-pixel patterns.

**Architecture:** One file, `src/components/CalendarHome.cpp`. Existing scaled renderer calls already draw at panel pixels (`fillRoundedRect`, `drawRoundedRect`); only the 75% punch-out and the dashed frame use `PhysicalScope` directly.

**Tech Stack:** C++ / GfxRenderer, PlatformIO env `metalio_eink4_plus`.

**Spec:** `docs/superpowers/specs/2026-11-25-calendar-day-tiles-design.md`

> **Note:** this plan records the first iteration. The design changed on the device; the spec is authoritative.

## Global Constraints

- No renderer, SDK, threshold (`DARK_MINUTES` 60, `BLACK_MINUTES` 120) or layout change.
- No heap allocation.
- Levels: unread past = thin outline; 1–59 min = 50% (black bold number); 60–119 = 75% (white number); 120+ = black (white number); today = 2 px outer frame; selected = dashed outer frame.

## Review Focus

- `drawPixel` under UI scale draws 1–2 px blocks (`GfxRenderer.cpp:1022`); every pattern pixel must be drawn inside `PhysicalScope`.
- 75% punch-out over the tile's bounding box must not mark paper outside the rounded corners (white on white is harmless; verify no gray-frame ink change).
- Today that is also read (any fill) still shows its frame and a readable number.
- Small tiles (`disc` down to 16) keep a positive radius and visible frame.
- Selected day equal to today: today's frame wins, as before.

No host test covers on-screen drawing; proof is the firmware build plus the owner's hardware check.

---

### Task 1: Rounded-square day tiles

**Files:**
- Modify: `src/components/CalendarHome.cpp:57-108` (helpers), `:378-408` (day loop)

**Interfaces:**
- Produces (anonymous namespace): `void fillTile(const GfxRenderer&, int x, int y, int size, int radius, uint32_t minutes)`; `void drawDashedFrame(const GfxRenderer&, int x, int y, int size, int radius)`.
- Removes: `halfChord`, `fillDisc`, `fillDiscFaint`, `drawRing` (no other users; check with grep).

- [ ] **Step 1:** Implement `fillTile`: 50% = `fillRoundedRect(..., Color::DarkGray)`; 75% = `fillRoundedRect(..., Color::Black)` then, inside `PhysicalScope` after `physicalRect`, `drawPixel(x, y, false)` where both physical coords are odd; 120+ = `Color::Black`.
- [ ] **Step 2:** Implement `drawDashedFrame`: inside `PhysicalScope`, 2x2 physical dots every 6 physical px along the four straight edges (corners excluded).
- [ ] **Step 3:** Day loop: tile `size = disc`, `radius = size / 4`, top-left at `cx - size/2, cy - size/2`. Unread past = `drawRoundedRect(..., 1, radius, true)`. Today = `drawRoundedRect` offset 4, line 2. Selected (not today) = `drawDashedFrame` offset 3. Number white when `m >= DARK_MINUTES`, bold when `m > 0 || d == today`. Update the comment above the loop.
- [ ] **Step 4:** Build: `PLATFORMIO_CORE_DIR=~/.platformio-eink4plus ~/.platformio-eink4plus/penv/bin/pio run -e metalio_eink4_plus`. Expected: SUCCESS, flash usage reported.
- [ ] **Step 5:** `clang-format` the file per repo rules; owner flashes and checks the spec's verification list. Commit only on owner approval.
