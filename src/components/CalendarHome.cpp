#include "CalendarHome.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "CrossPointSettings.h"
#include "ReadingLogStore.h"
#include "UITheme.h"
#include "fontIds.h"
#include "util/BookProgress.h"

namespace {
constexpr int TOOL_BAR_HEIGHT = 80;
constexpr int TILE_HEIGHT = 136;
constexpr int TILE_RADIUS = 14;
constexpr int TILE_PAD = 12;
constexpr int GAP = 12;
constexpr int SECTION_GAP = 16;
constexpr int DOT_MINUTES = 10;
constexpr int DOT_COLS = 6;
constexpr int DOT_ROWS = 4;
constexpr int DISC_MAX = 52;
// Past-day fill by minutes read: none (75%-dot ring), 25%, 50%, then black.
constexpr uint32_t DARK_MINUTES = 60;
constexpr uint32_t BLACK_MINUTES = 120;
constexpr unsigned long DAY_INFO_MS = 10000;
constexpr const char* NO_VALUE = "-";

// 5x7 dot-matrix glyphs, one row per byte with bit 4 as the left column.
constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 7;
constexpr int GLYPH_DASH = 10;
constexpr int GLYPH_COLON = 11;
constexpr uint8_t GLYPHS[][GLYPH_H] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},  // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},  // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},  // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},  // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},  // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},  // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},  // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},  // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},  // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},  // 9
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},  // -
    {0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00},  // : (middle column)
};

// A disc of the given diameter centered on (cx, cy).
void fillDot(const GfxRenderer& renderer, const int cx, const int cy, const int d, const Color color) {
  renderer.fillCircle(cx - d / 2, cy - d / 2, d, color);
}

// A ring of the given diameter centered on (cx, cy).
void drawRing(const GfxRenderer& renderer, const int cx, const int cy, const int d, const int lineWidth,
              const Color color) {
  renderer.drawCircle(cx - d / 2, cy - d / 2, d, lineWidth, color);
}

// UI units for a 2-pixel line at scales 1.0 and 1.5: a single pixel breaks on the
// glass, and in 75% dots it breaks into dashes.
int thinLine(const GfxRenderer& renderer) {
  return std::max(1, static_cast<int>(std::lround(2.0f / renderer.uiScale())));
}

// One glyph as white dots on a grid of square cells: column c is centered at
// x + (c + 0.5) * cell. The cell may be fractional, so the clock passes panel
// pixels and every glyph shares one grid.
void drawGlyph(const GfxRenderer& renderer, const int glyph, const int x, const int y, const float cell,
               const int startCol, const int d) {
  for (int row = 0; row < GLYPH_H; ++row) {
    for (int col = 0; col < GLYPH_W; ++col) {
      if (GLYPHS[glyph][row] & (0x10 >> col)) {
        const int cx = x + static_cast<int>(std::lround((static_cast<float>(startCol + col) + 0.5f) * cell));
        const int cy = y + static_cast<int>(std::lround((static_cast<float>(row) + 0.5f) * cell));
        fillDot(renderer, cx, cy, d, Color::White);
      }
    }
  }
}

// A day's disc shaded by minutes read: 25%, 50%, then black. Single-pixel
// dots spread on the glass, so levels need wide steps to look apart.
Color dayColor(const uint32_t minutes) {
  return minutes >= BLACK_MINUTES ? Color::Black : minutes >= DARK_MINUTES ? Color::DarkGray : Color::LightGray;
}

// A ring of small dots, every 15 degrees.
void drawDottedRing(const GfxRenderer& renderer, const int cx, const int cy, const int r) {
  for (int a = 0; a < 360; a += 15) {
    const float rad = static_cast<float>(a) * 3.14159265f / 180.0f;
    const int px = cx + static_cast<int>(std::lround(std::cos(rad) * static_cast<float>(r)));
    const int py = cy + static_cast<int>(std::lround(std::sin(rad) * static_cast<float>(r)));
    fillDot(renderer, px, py, 2, Color::Black);
  }
}

void drawCentered(const GfxRenderer& renderer, const int fontId, const int cx, const int cy, const char* text,
                  const bool black, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int w = renderer.getTextWidth(fontId, text, style);
  renderer.drawText(fontId, cx - w / 2, cy - renderer.getLineHeight(fontId) / 2, text, black, style);
}

int daysInMonth(const struct tm& date) {
  constexpr int DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const int year = date.tm_year + 1900;
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return date.tm_mon == 1 && leap ? 29 : DAYS[date.tm_mon];
}
}  // namespace

void CalendarHome::loadProgress(const std::vector<RecentBook>& books) {
  currentProgress = books.empty() ? -1 : static_cast<int8_t>(loadBookProgress(books[0].path));
}

bool CalendarHome::needsRedraw() const {
  if (selectedDay > 0 && millis() - selectedAt >= DAY_INFO_MS) return true;
  struct tm now;
  if (!ReadingLogStore::localDate(now)) return false;
  return now.tm_hour * 60 + now.tm_min != shownMinute;
}

bool CalendarHome::selectDayAt(const int x, const int y) {
  if (dayCount == 0 || x < gridX || x >= gridX + 7 * colW || y < gridY) return false;
  const int slot = (y - gridY) / rowH * 7 + (x - gridX) / colW;
  const int day = slot - firstWeekday + 1;
  if (day < 1 || day > today) return false;
  selectedDay = day == selectedDay ? 0 : day;
  selectedAt = millis();
  return true;
}

bool CalendarHome::takeHourTurn() {
  const bool turned = hourTurned;
  hourTurned = false;
  return turned;
}

void CalendarHome::render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools,
                          const int toolCount, const int selected) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  const int bookCount = std::min(static_cast<int>(books.size()), MAX_BOOKS);
  const int shownTools = std::min(toolCount, MAX_TOOLS);
  targets.fill(Rect{});
  targetCount = bookCount + shownTools;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.batteryBarHeight}, nullptr,
                 nullptr, false);

  struct tm date;
  const bool hasDate = ReadingLogStore::localDate(date);
  const int minute = hasDate ? date.tm_hour * 60 + date.tm_min : -1;
  if (minute >= 0 && shownMinute >= 0 && minute / 60 != shownMinute / 60) hourTurned = true;
  shownMinute = static_cast<int16_t>(minute);
  if (selectedDay > 0 && millis() - selectedAt >= DAY_INFO_MS) selectedDay = 0;
  const int minutesRead =
      hasDate ? static_cast<int>((READING_LOG.secondsOn(ReadingLogStore::dayNumber(date)) + 30) / 60) : -1;

  const int left = safe.x + metrics.contentSidePadding;
  const int width = safe.width - 2 * metrics.contentSidePadding;
  const int top = std::max(safe.y, metrics.topPadding + metrics.batteryBarHeight) + 8;

  const Rect toolBar{safe.x, safe.y + safe.height - TOOL_BAR_HEIGHT, safe.width, TOOL_BAR_HEIGHT};
  homeui::drawToolBar(renderer, tools, shownTools, bookCount, selected, toolBar, targets.data());
  const int bodyBottom = toolBar.y - GAP;

  // Portrait stacks tiles, month and book; landscape puts the month beside them.
  Rect tiles;
  Rect month;
  Rect book;
  if (landscape) {
    const int leftW = width / 2;
    tiles = Rect{left, top, leftW, TILE_HEIGHT};
    book = Rect{left, bodyBottom - COVER_HEIGHT, leftW, COVER_HEIGHT};
    month = Rect{left + leftW + 2 * GAP, top, width - leftW - 2 * GAP, bodyBottom - top};
  } else {
    tiles = Rect{left, top, width, TILE_HEIGHT};
    book = Rect{left, bodyBottom - COVER_HEIGHT, width, COVER_HEIGHT};
    const int monthY = tiles.y + tiles.height + SECTION_GAP;
    month = Rect{left, monthY, width, book.y - 2 * SECTION_GAP - monthY};
    // Dashed seam between the calendar and the book, like a torn-off page.
    const int seamY = book.y - SECTION_GAP;
    for (int x = left; x < left + width; x += 10) renderer.fillRect(x, seamY, std::min(6, left + width - x), 2, true);
  }

  const int clockW = tiles.width * 3 / 5;
  drawClock(renderer, Rect{tiles.x, tiles.y, clockW, tiles.height}, hasDate ? &date : nullptr);
  drawReadToday(renderer, Rect{tiles.x + clockW + GAP, tiles.y, tiles.width - clockW - GAP, tiles.height}, minutesRead);
  drawMonth(renderer, month, hasDate ? &date : nullptr);

  if (bookCount > 0) {
    homeui::drawFeaturedBook(renderer, books[0], currentProgress, COVER_HEIGHT, book, selected == 0, false);
    targets[0] = book;
  } else {
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, book, UI_12_FONT_ID, book.y + book.height / 2 - lineHeight,
                              tr(STR_NO_OPEN_BOOK), true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, book, UI_10_FONT_ID, book.y + book.height / 2 + 4, tr(STR_HOME_EMPTY_HINT));
  }
}

void CalendarHome::drawClock(const GfxRenderer& renderer, const Rect rect, const struct tm* date) const {
  renderer.fillRoundedRect(rect.x, rect.y, rect.width, rect.height, TILE_RADIUS, Color::Black);

  // HH:MM on one row within the padded tile. Columns:
  // digit, space, digit, space, colon, space, digit, space, digit.
  constexpr int COLS_TOTAL = 4 * GLYPH_W + 5;
  constexpr int START[4] = {0, GLYPH_W + 1, 2 * GLYPH_W + 4, 3 * GLYPH_W + 5};
  constexpr int COLON_COL = 2 * GLYPH_W + 2;
  // The grid is laid out in panel pixels: whole UI units would round the cell
  // and shrink the clock, or space its dots unevenly.
  int x = rect.x + TILE_PAD;
  int y = rect.y + TILE_PAD;
  int w = rect.width - 2 * TILE_PAD;
  int h = rect.height - 2 * TILE_PAD;
  renderer.physicalRect(x, y, w, h);
  const GfxRenderer::PhysicalScope physical(renderer);
  // Square cells as large as the padded tile allows, the grid centered in it.
  const float cell = std::min(static_cast<float>(w) / COLS_TOTAL, static_cast<float>(h) / GLYPH_H);
  x += static_cast<int>(std::lround((static_cast<float>(w) - cell * COLS_TOTAL) / 2));
  y += static_cast<int>(std::lround((static_cast<float>(h) - cell * GLYPH_H) / 2));
  const int d = 2 * std::max(1, static_cast<int>(cell * 0.42f)) + 1;
  int glyphs[4] = {GLYPH_DASH, GLYPH_DASH, GLYPH_DASH, GLYPH_DASH};
  if (date) {
    int hour = date->tm_hour;
    if (SETTINGS.clockFormat == 1) hour = hour % 12 == 0 ? 12 : hour % 12;
    glyphs[0] = hour / 10;
    glyphs[1] = hour % 10;
    glyphs[2] = date->tm_min / 10;
    glyphs[3] = date->tm_min % 10;
  }
  for (int i = 0; i < 4; ++i) drawGlyph(renderer, glyphs[i], x, y, cell, START[i], d);
  // The colon glyph lights its middle column; put that on the colon column.
  if (date) drawGlyph(renderer, GLYPH_COLON, x, y, cell, COLON_COL - GLYPH_W / 2, d);
}

void CalendarHome::drawReadToday(const GfxRenderer& renderer, const Rect rect, const int minutes) const {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, TILE_RADIUS, true);

  const int innerX = rect.x + TILE_PAD;
  const int innerW = rect.width - 2 * TILE_PAD;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  char value[32];
  if (minutes >= 0) {
    snprintf(value, sizeof(value), "%d %s", minutes, tr(STR_UNIT_MINUTES));
  } else {
    snprintf(value, sizeof(value), "%s", NO_VALUE);
  }
  const int valueW = renderer.getTextWidth(UI_10_FONT_ID, value, EpdFontFamily::BOLD);
  const int labelY = rect.y + TILE_PAD - 2;
  // "Today" leads the value; the value alone when the tile is too narrow for both.
  const char* label = tr(STR_TODAY_TITLE);
  const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label);
  if (labelW + 6 + valueW <= innerW) {
    renderer.drawText(UI_10_FONT_ID, innerX, labelY, label, true);
    renderer.drawText(UI_10_FONT_ID, innerX + labelW + 6, labelY, value, true, EpdFontFamily::BOLD);
  } else {
    renderer.drawText(UI_10_FONT_ID, innerX, labelY, value, true, EpdFontFamily::BOLD);
  }

  // One dot per ten minutes read, filled in reading order.
  const int gridY = labelY + lineH + 6;
  const int gridH = rect.y + rect.height - TILE_PAD - gridY;
  const int pitch = std::min(innerW / DOT_COLS, gridH / DOT_ROWS);
  if (pitch < 8) return;
  // Spread the dots to the tile's padding on every side; the spare room goes
  // between them in equal steps.
  const int d = std::max(6, pitch * 4 / 5);
  const int stepX = (innerW - d) / (DOT_COLS - 1);
  const int stepY = (gridH - d) / (DOT_ROWS - 1);
  const int x0 = innerX + (innerW - d - stepX * (DOT_COLS - 1)) / 2 + d / 2;
  const int y0 = gridY + (gridH - d - stepY * (DOT_ROWS - 1)) / 2 + d / 2;
  const int filled = std::max(0, minutes) / DOT_MINUTES;
  for (int i = 0; i < DOT_COLS * DOT_ROWS; ++i) {
    const int cx = x0 + (i % DOT_COLS) * stepX;
    const int cy = y0 + (i / DOT_COLS) * stepY;
    if (i < filled) {
      fillDot(renderer, cx, cy, d, Color::Black);
    } else {
      drawRing(renderer, cx, cy, d, thinLine(renderer), Color::Black);
    }
  }
}

void CalendarHome::drawMonth(const GfxRenderer& renderer, const Rect rect, const struct tm* date) {
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  dayCount = 0;
  if (!date) {
    UITheme::drawCenteredWrappedText(renderer, rect, UI_12_FONT_ID, tr(STR_TODAY_CLOCK_HINT), 3, true,
                                     EpdFontFamily::BOLD);
    return;
  }

  const int days = daysInMonth(*date);
  uint32_t minutes[31];
  uint32_t monthTotal = 0;
  struct tm day = *date;
  for (int d = 1; d <= days; ++d) {
    day.tm_mday = d;
    const uint32_t seconds = d <= date->tm_mday ? READING_LOG.secondsOnDate(day) : 0;
    minutes[d - 1] = seconds >= ReadingLogStore::READING_DAY_SECONDS ? (seconds + 30) / 60 : 0;
    monthTotal += minutes[d - 1];
  }

  // Month pill and its total on the left; the tapped day's time, else the
  // reading streak, on the right. Text that does not fit is left out.
  const char* monthName = I18N.get(homeui::MONTHS[date->tm_mon]);
  const int pillH = smallH + 10;
  const int pillW = renderer.getTextWidth(UI_10_FONT_ID, monthName, EpdFontFamily::BOLD) + 24;
  renderer.fillRoundedRect(rect.x, rect.y, pillW, pillH, pillH / 2, Color::Black);
  renderer.drawText(UI_10_FONT_ID, rect.x + 12, rect.y + 5, monthName, false, EpdFontFamily::BOLD);
  int usedW = pillW;
  if (monthTotal > 0) {
    char total[48];
    homeui::formatDuration(total, sizeof(total), monthTotal);
    const int totalW = renderer.getTextWidth(UI_10_FONT_ID, total, EpdFontFamily::BOLD);
    if (pillW + 10 + totalW <= rect.width) {
      renderer.drawText(UI_10_FONT_ID, rect.x + pillW + 10, rect.y + 5, total, true, EpdFontFamily::BOLD);
      usedW += 10 + totalW;
    }
  }
  const int todayNumber = ReadingLogStore::dayNumber(*date);
  char side[96] = "";
  if (selectedDay > 0) {
    char duration[48];
    homeui::formatDuration(duration, sizeof(duration), minutes[selectedDay - 1]);
    snprintf(side, sizeof(side), "%d %s %s", selectedDay, monthName, duration);
    if (pillW + 16 + renderer.getTextWidth(UI_10_FONT_ID, side, EpdFontFamily::BOLD) > rect.width) {
      snprintf(side, sizeof(side), "%d: %s", selectedDay, duration);
    }
  } else if (const int streak = READING_LOG.streak(todayNumber); streak > 0) {
    snprintf(side, sizeof(side), "%s %d %s", tr(STR_CALENDAR_STREAK), streak, tr(STR_UNIT_DAYS));
  }
  if (side[0] != '\0') {
    const auto style = selectedDay > 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    const int sideW = renderer.getTextWidth(UI_10_FONT_ID, side, style);
    // A tapped day's time is the answer the reader asked for; it outranks the month total.
    const int need = (selectedDay > 0 ? pillW : usedW) + 16 + sideW;
    if (need <= rect.width) {
      if (selectedDay > 0 && usedW + 16 + sideW > rect.width) {
        renderer.fillRect(rect.x + pillW + 1, rect.y, rect.width - pillW - 1, pillH, false);
      }
      renderer.drawText(UI_10_FONT_ID, rect.x + rect.width - sideW, rect.y + 5, side, true, style);
    }
  }

  // Weeks start on Sunday, as on Thai wall calendars.
  colW = rect.width / 7;
  const int headerY = rect.y + pillH + 10;
  for (int i = 0; i < 7; ++i) {
    drawCentered(renderer, UI_10_FONT_ID, rect.x + colW * i + colW / 2, headerY + smallH / 2,
                 I18N.get(homeui::WEEKDAYS_SHORT[i]), true, EpdFontFamily::BOLD);
  }

  firstWeekday = ReadingLogStore::weekday(todayNumber - (date->tm_mday - 1));
  const int rows = (firstWeekday + days + 6) / 7;
  gridX = rect.x;
  gridY = headerY + smallH + 6;
  rowH = std::min(colW, (rect.y + rect.height - gridY) / rows);
  const int disc = std::min(DISC_MAX, std::min(colW, rowH) - 8);
  if (disc < 16) return;
  dayCount = days;
  today = date->tm_mday;

  // Days darken with time read: 25%, 50%, then black. Past days with no
  // counted reading get a 75%-dot ring; days to come stay bare. Today wears a
  // solid outer ring and a badge; a tapped day a dotted ring.
  for (int d = 1; d <= days; ++d) {
    const int slot = firstWeekday + d - 1;
    const int cx = gridX + colW * (slot % 7) + colW / 2;
    const int cy = gridY + rowH * (slot / 7) + rowH / 2;
    const uint32_t m = minutes[d - 1];
    if (m > 0) {
      fillDot(renderer, cx, cy, disc, dayColor(m));
    } else if (d < today) {
      drawRing(renderer, cx, cy, disc, thinLine(renderer), Color::DarkerGray);
    }
    if (d == today) {
      drawRing(renderer, cx, cy, disc + 8, 2, Color::Black);
    } else if (d == selectedDay) {
      drawDottedRing(renderer, cx, cy, disc / 2 + 3);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", d);
    drawCentered(renderer, UI_10_FONT_ID, cx, cy, label, m < BLACK_MINUTES,
                 m > 0 || d == today ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    if (d == today) {
      // A badge on the ring at the top right, ringed in paper so it shows on
      // any fill.
      const int badge = std::max(8, disc / 4);
      const int offset = static_cast<int>(std::lround(static_cast<float>(disc / 2 + 2) * 0.7071f));
      fillDot(renderer, cx + offset, cy - offset, badge + 4, Color::White);
      fillDot(renderer, cx + offset, cy - offset, badge, Color::Black);
    }
  }
}

int CalendarHome::hitTest(const int x, const int y) const {
  for (int i = 0; i < targetCount; ++i) {
    const Rect& r = targets[i];
    if (r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}
