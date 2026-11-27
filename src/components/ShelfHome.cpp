#include "ShelfHome.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>

#include "ReadingLogStore.h"
#include "UITheme.h"
#include "fontIds.h"
#include "util/BookProgress.h"
#include "util/ReadingHistory.h"

namespace {
constexpr int CARD_PAD = 16;
constexpr int CARD_RADIUS = 18;
constexpr int COVER_RADIUS = 8;
constexpr int CAPSULE_HEIGHT = 44;
constexpr int CAPSULE_GAP = 14;
constexpr int CARD_HEIGHT = CARD_PAD + ShelfHome::FEATURED_COVER_HEIGHT + CAPSULE_GAP + CAPSULE_HEIGHT + CARD_PAD;
constexpr int FEATURED_COVER_WIDTH = ShelfHome::FEATURED_COVER_HEIGHT * 2 / 3;
constexpr int TOOL_BAR_HEIGHT = 56;
constexpr int TOOL_ICON = 32;
constexpr int TRACK_HEIGHT = 8;
constexpr int SECTION_GAP = 16;
constexpr int DOT_SIZE = 10;
constexpr int DOT_GAP = 10;
constexpr int DOT_ROW = DOT_SIZE + 12;
// Below this the recent row is left out rather than drawn as slivers.
constexpr int MIN_RECENT_COVER = 60;
static_assert(shelf::HEAT_WEEKS * 7 <= ReadingHistory::DAYS, "activity grid reaches past the reading history");

struct Layout {
  Rect card;
  Rect recent;  // heading, covers, titles, progress and page dots
  Rect activity;
  Rect tools;
  int columns = 3;
  int coverHeight = 0;  // 0 when the recent row does not fit
};

// Portrait only: readers restore portrait on exit (ReaderActivity::onExit).
Layout layoutFor(const GfxRenderer& renderer) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  const int left = safe.x + metrics.contentSidePadding;
  const int width = safe.width - 2 * metrics.contentSidePadding;
  const int top = std::max(safe.y, metrics.topPadding + metrics.batteryBarHeight) + 8;
  Layout l;
  l.tools = Rect{left, safe.y + safe.height - TOOL_BAR_HEIGHT - 8, width, TOOL_BAR_HEIGHT};
  l.activity = Rect{left, l.tools.y - SECTION_GAP - shelf::heatGridHeight(), width, shelf::heatGridHeight()};
  l.card = Rect{left, top, width, CARD_HEIGHT};
  const int recentY = l.card.y + l.card.height + SECTION_GAP;
  l.recent = Rect{left, recentY, width, l.activity.y - SECTION_GAP - recentY};

  l.columns = shelf::recentColumns(width);
  const int tileW = (width - (l.columns - 1) * shelf::TILE_GAP) / l.columns;
  const int textH = 6 + renderer.getLineHeight(UI_10_FONT_ID) + 6 + renderer.getLineHeight(UI_10_FONT_ID);
  const int headingH = renderer.getLineHeight(UI_12_FONT_ID) + 10;
  const int coverH = std::min(tileW * 3 / 2, l.recent.height - headingH - textH - DOT_ROW);
  l.coverHeight = coverH >= MIN_RECENT_COVER ? coverH : 0;
  return l;
}

void drawPercentRow(const GfxRenderer& renderer, const int x, const int y, const int w, const int percent) {
  char text[8];
  snprintf(text, sizeof(text), "%d%%", percent);
  const int textW = renderer.getTextWidth(UI_10_FONT_ID, text, EpdFontFamily::BOLD);
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int trackW = w - textW - 8;
  if (trackW > 0) {
    homeui::drawProgressTrack(renderer, Rect{x, y + (lineH - TRACK_HEIGHT) / 2, trackW, TRACK_HEIGHT}, percent);
  }
  renderer.drawText(UI_10_FONT_ID, x + w - textW, y, text, true, EpdFontFamily::BOLD);
}
}  // namespace

int ShelfHome::coverHeightFor(const GfxRenderer& renderer, const size_t index) {
  return index == 0 ? FEATURED_COVER_HEIGHT : layoutFor(renderer).coverHeight;
}

int ShelfHome::maxBooks(const GfxRenderer& renderer) { return layoutFor(renderer).coverHeight > 0 ? MAX_BOOKS : 1; }

void ShelfHome::loadProgress(const std::vector<RecentBook>& books) {
  const size_t count = std::min(books.size(), progress.size());
  for (size_t i = 0; i < count; ++i) progress[i] = static_cast<int8_t>(loadBookProgress(books[i].path));
}

void ShelfHome::render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools,
                       const int toolCount, const int selected) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Layout l = layoutFor(renderer);
  const int bookCount = std::min(static_cast<int>(books.size()), l.coverHeight > 0 ? MAX_BOOKS : 1);
  const int shownTools = std::min(toolCount, MAX_TOOLS);
  targets.fill(Rect{});
  targetCount = bookCount + shownTools;
  recentTotal = shelf::recentCount(bookCount);
  columns = l.columns;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.batteryBarHeight}, nullptr,
                 nullptr, false);

  drawCard(renderer, bookCount > 0 ? &books[0] : nullptr, l.card, selected == 0 && bookCount > 0);
  if (bookCount > 0) targets[0] = l.card;
  if (recentTotal > 0) drawRecent(renderer, books, l.recent, l.coverHeight, selected);
  drawActivity(renderer, l.activity);
  drawToolBar(renderer, tools, shownTools, bookCount, selected, l.tools);
}

int ShelfHome::turnPage(const int selected, const int dir) {
  const int pages = shelf::recentPages(recentTotal, columns);
  const int next = page + dir;
  if (next < 0 || next >= pages) return -1;
  page = next;
  // A selected cover follows the page; other selections stay where they are.
  return selected >= 1 && selected <= recentTotal ? 1 + page * columns : selected;
}

void ShelfHome::drawCard(const GfxRenderer& renderer, const RecentBook* book, const Rect rect,
                         const bool selected) const {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, selected ? 4 : 2, CARD_RADIUS, true);
  if (!book) {
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, rect, UI_12_FONT_ID, rect.y + rect.height / 2 - lineH, tr(STR_NO_OPEN_BOOK),
                              true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, rect, UI_10_FONT_ID, rect.y + rect.height / 2 + 4, tr(STR_HOME_EMPTY_HINT));
    return;
  }

  const Rect cover{rect.x + CARD_PAD, rect.y + CARD_PAD, FEATURED_COVER_WIDTH, FEATURED_COVER_HEIGHT};
  homeui::drawCover(renderer, *book, FEATURED_COVER_HEIGHT, cover, COVER_RADIUS);

  const int x = cover.x + cover.width + CARD_PAD;
  const int w = rect.x + rect.width - CARD_PAD - x;
  const int titleH = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  if (w > 0) {
    // Bottom-up inside the cover height: reading-time line, then progress;
    // the title and author get what is left above them.
    int bottom = cover.y + cover.height;
    struct tm date;
    const bool hasDate = ReadingLogStore::localDate(date);
    const int32_t today = hasDate ? ReadingLogStore::dayNumber(date) : -1;
    const int minutes = hasDate ? static_cast<int>((READING_LOG.secondsOn(today) + 30) / 60) : 0;
    const int streak = hasDate ? READING_LOG.streak(today) : 0;
    if (shelf::showReadingLine(hasDate, minutes, streak)) {
      char duration[32];
      homeui::formatDuration(duration, sizeof(duration), static_cast<uint32_t>(minutes));
      char line[160];
      snprintf(line, sizeof(line), "%s %s · %s %d %s", tr(STR_SHELF_READ_TODAY), duration, tr(STR_SHELF_STREAK), streak,
               tr(STR_UNIT_DAYS));
      bottom -= smallH;
      renderer.drawText(UI_10_FONT_ID, x, bottom, renderer.truncatedText(UI_10_FONT_ID, line, w).c_str(), true);
      bottom -= 6;
    }
    if (progress[0] >= 0) {
      bottom -= smallH;
      drawPercentRow(renderer, x, bottom, w, progress[0]);
      bottom -= 6;
    }

    const int space = bottom - cover.y;
    const bool showAuthor = !book->author.empty() && space >= titleH + smallH + 2;
    const int titleLines = std::clamp((space - (showAuthor ? smallH + 2 : 0)) / std::max(1, titleH), 1, 2);
    int y = cover.y;
    for (const auto& line :
         renderer.wrappedText(UI_12_FONT_ID, book->title.c_str(), w, titleLines, EpdFontFamily::BOLD)) {
      renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += titleH;
    }
    if (showAuthor) {
      renderer.drawText(UI_10_FONT_ID, x, y + 2, renderer.truncatedText(UI_10_FONT_ID, book->author.c_str(), w).c_str(),
                        true);
    }
  }

  // Resume capsule across the card, always black like a primary button.
  const Rect capsule{rect.x + CARD_PAD, cover.y + cover.height + CAPSULE_GAP, rect.width - 2 * CARD_PAD,
                     CAPSULE_HEIGHT};
  renderer.fillRoundedRect(capsule.x, capsule.y, capsule.width, capsule.height, CAPSULE_HEIGHT / 2, Color::Black);
  const char* label = tr(STR_CONTINUE_READING);
  const int labelX = capsule.x + 22;
  const int labelW = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, labelX, capsule.y + (CAPSULE_HEIGHT - titleH) / 2, label, false,
                    EpdFontFamily::BOLD);
  const int arrowW = renderer.getTextWidth(UI_12_FONT_ID, ">", EpdFontFamily::BOLD);
  const int arrowX = capsule.x + capsule.width - 22 - arrowW;
  renderer.drawText(UI_12_FONT_ID, arrowX, capsule.y + (CAPSULE_HEIGHT - titleH) / 2, ">", false, EpdFontFamily::BOLD);
  const int titleSpace = arrowX - 10 - (labelX + labelW + 20);
  if (titleSpace > 40) {
    const std::string title = renderer.truncatedText(UI_10_FONT_ID, book->title.c_str(), titleSpace);
    const int textW = renderer.getTextWidth(UI_10_FONT_ID, title.c_str());
    renderer.drawText(UI_10_FONT_ID, arrowX - 10 - textW, capsule.y + (CAPSULE_HEIGHT - smallH) / 2, title.c_str(),
                      false);
  }
}

void ShelfHome::drawRecent(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Rect rect,
                           const int coverHeight, const int selected) {
  const int pages = shelf::recentPages(recentTotal, columns);
  if (selected >= 1 && selected <= recentTotal) page = (selected - 1) / columns;
  page = std::clamp(page, 0, pages - 1);

  renderer.drawText(UI_12_FONT_ID, rect.x, rect.y, tr(STR_SHELF_RECENT), true, EpdFontFamily::BOLD);
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  const int tileW = (rect.width - (columns - 1) * shelf::TILE_GAP) / columns;
  const int coverW = std::min(coverHeight * 2 / 3, tileW);
  const int tileY = rect.y + renderer.getLineHeight(UI_12_FONT_ID) + 10;
  const int tileH = coverHeight + 6 + smallH + 6 + smallH;

  const int first = 1 + page * columns;
  const int last = std::min(recentTotal, page * columns + columns);
  for (int index = first; index <= last; ++index) {
    const RecentBook& book = books[index];
    const int column = index - first;
    const Rect tile{rect.x + column * (tileW + shelf::TILE_GAP), tileY, tileW, tileH};
    homeui::drawCover(renderer, book, coverHeight, Rect{tile.x + (tileW - coverW) / 2, tile.y, coverW, coverHeight},
                      COVER_RADIUS);
    int y = tile.y + coverHeight + 6;
    renderer.drawText(UI_10_FONT_ID, tile.x, y,
                      renderer.truncatedText(UI_10_FONT_ID, book.title.c_str(), tileW).c_str(), true);
    y += smallH + 6;
    if (progress[index] >= 0) drawPercentRow(renderer, tile.x, y, tileW, progress[index]);
    if (selected == index) {
      renderer.drawRoundedRect(tile.x - 6, tile.y - 6, tile.width + 12, tile.height + 12, 3, 12, true);
    }
    targets[index] = tile;
  }

  if (pages > 1) {
    const int dotsW = pages * DOT_SIZE + (pages - 1) * DOT_GAP;
    int x = rect.x + (rect.width - dotsW) / 2;
    const int y = tileY + tileH + 12;
    for (int p = 0; p < pages; ++p) {
      if (p == page) {
        renderer.fillRoundedRect(x, y, DOT_SIZE, DOT_SIZE, DOT_SIZE / 2, Color::Black);
      } else {
        renderer.drawRoundedRect(x, y, DOT_SIZE, DOT_SIZE, 1, DOT_SIZE / 2, true);
      }
      x += DOT_SIZE + DOT_GAP;
    }
  }
}

void ShelfHome::drawActivity(const GfxRenderer& renderer, const Rect rect) const {
  struct tm date;
  if (!ReadingLogStore::localDate(date)) {
    UITheme::drawCenteredWrappedText(renderer, rect, UI_10_FONT_ID, tr(STR_TODAY_CLOCK_HINT), 2, true);
    return;
  }
  const int32_t today = ReadingLogStore::dayNumber(date);
  // Columns are weeks from Sunday; the last column is this week.
  const int32_t firstDay = today - ReadingLogStore::weekday(today) - (shelf::HEAT_WEEKS - 1) * 7;
  constexpr int step = shelf::HEAT_CELL + shelf::HEAT_GAP;
  uint32_t total = 0;
  int readingDays = 0;
  for (int week = 0; week < shelf::HEAT_WEEKS; ++week) {
    for (int weekday = 0; weekday < 7; ++weekday) {
      const int32_t day = firstDay + week * 7 + weekday;
      if (day > today) break;
      const int minutes = READING_LOG.historyMinutes(day);
      total += minutes;
      if (minutes > 0) ++readingDays;
      const int x = rect.x + week * step;
      const int y = rect.y + weekday * step;
      constexpr int cell = shelf::HEAT_CELL;
      switch (shelf::heatLevel(minutes)) {
        case 0:
          renderer.drawRoundedRect(x, y, cell, cell, 1, 2, true);
          break;
        case 1:
          renderer.fillRectDither(x, y, cell, cell, Color::LightGray);
          renderer.drawRoundedRect(x, y, cell, cell, 1, 2, true);
          break;
        case 2:
          renderer.fillRectDither(x, y, cell, cell, Color::DarkGray);
          renderer.drawRoundedRect(x, y, cell, cell, 1, 2, true);
          break;
        default:
          renderer.fillRoundedRect(x, y, cell, cell, 2, Color::Black);
          break;
      }
    }
  }

  // Summary beside the grid: title, total time, reading days.
  const int x = rect.x + shelf::heatGridWidth() + 24;
  const int w = rect.x + rect.width - x;
  if (w <= 40) return;
  const int smallH = renderer.getLineHeight(UI_10_FONT_ID);
  char duration[32];
  homeui::formatDuration(duration, sizeof(duration), total);
  char totalLine[64];
  snprintf(totalLine, sizeof(totalLine), "%s %s", tr(STR_SHELF_TOTAL), duration);
  char daysLine[64];
  snprintf(daysLine, sizeof(daysLine), "%s %d %s", tr(STR_SHELF_READ_DAYS), readingDays, tr(STR_UNIT_DAYS));
  int y = rect.y + (rect.height - 3 * smallH - 8) / 2;
  renderer.drawText(UI_10_FONT_ID, x, y,
                    renderer.truncatedText(UI_10_FONT_ID, tr(STR_SHELF_LAST_3_MONTHS), w, EpdFontFamily::BOLD).c_str(),
                    true, EpdFontFamily::BOLD);
  y += smallH + 8;
  renderer.drawText(UI_10_FONT_ID, x, y, renderer.truncatedText(UI_10_FONT_ID, totalLine, w).c_str(), true);
  y += smallH;
  renderer.drawText(UI_10_FONT_ID, x, y, renderer.truncatedText(UI_10_FONT_ID, daysLine, w).c_str(), true);
}

void ShelfHome::drawToolBar(const GfxRenderer& renderer, const Tool* tools, const int count, const int firstIndex,
                            const int selected, const Rect rect) {
  if (count <= 0) return;
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, 2, rect.height / 2, true);
  const int inner = rect.width - 8;
  const int active = selected - firstIndex;
  const bool hasActive = active >= 0 && active < count;

  // The selected tool widens to fit its label; the others share the rest.
  int activeW = 0;
  std::string activeLabel;
  if (hasActive) {
    const int maxW = inner / 2;
    activeLabel =
        renderer.truncatedText(UI_10_FONT_ID, tools[active].label, maxW - TOOL_ICON - 34, EpdFontFamily::BOLD);
    const int contentW = TOOL_ICON + 6 + renderer.getTextWidth(UI_10_FONT_ID, activeLabel.c_str(), EpdFontFamily::BOLD);
    activeW = std::max(inner / count, std::min(maxW, contentW + 28));
  }
  const int otherCount = hasActive ? count - 1 : count;
  const int otherW = otherCount > 0 ? (inner - activeW) / otherCount : 0;

  int x = rect.x + 4;
  const int cellY = rect.y + 4;
  const int cellH = rect.height - 8;
  for (int i = 0; i < count; ++i) {
    const bool isActive = hasActive && i == active;
    // The last cell takes the rounding remainder so the bar has no dead edge.
    const int cellW = i == count - 1 ? rect.x + 4 + inner - x : (isActive ? activeW : otherW);
    const uint8_t* icon = homeui::toolIcon(tools[i].icon);
    const int iconY = cellY + (cellH - TOOL_ICON) / 2;
    if (isActive) {
      renderer.fillRoundedRect(x, cellY, cellW, cellH, cellH / 2, Color::Black);
      const int labelW = renderer.getTextWidth(UI_10_FONT_ID, activeLabel.c_str(), EpdFontFamily::BOLD);
      const int contentW = (icon ? TOOL_ICON + 6 : 0) + labelW;
      int cx = x + (cellW - contentW) / 2;
      if (icon) {
        renderer.drawIcon(icon, cx, iconY, TOOL_ICON, false);
        cx += TOOL_ICON + 6;
      }
      renderer.drawText(UI_10_FONT_ID, cx, cellY + (cellH - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                        activeLabel.c_str(), false, EpdFontFamily::BOLD);
    } else if (icon) {
      renderer.drawIcon(icon, x + (cellW - TOOL_ICON) / 2, iconY, TOOL_ICON);
    } else {
      const std::string label = renderer.truncatedText(UI_10_FONT_ID, tools[i].label, cellW - 8);
      UITheme::drawCenteredText(renderer, Rect{x, cellY, cellW, cellH}, UI_10_FONT_ID,
                                cellY + (cellH - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label.c_str());
    }
    targets[firstIndex + i] = Rect{x, rect.y, cellW, rect.height};
    x += cellW;
  }
}

int ShelfHome::hitTest(const int x, const int y) const {
  for (int i = 0; i < targetCount; ++i) {
    const Rect& r = targets[i];
    if (r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}
