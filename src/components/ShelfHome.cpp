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
// Card height without its cover.
constexpr int CARD_CHROME = CARD_PAD + CAPSULE_GAP + CAPSULE_HEIGHT + CARD_PAD;
constexpr int TRACK_HEIGHT = 12;
constexpr int SECTION_GAP = 16;
constexpr int DOT_SIZE = 10;
constexpr int DOT_GAP = 10;
constexpr int DOT_ROW = DOT_SIZE + 12;
constexpr int SUMMARY_GAP = 12;
// Below this the recent row is left out rather than drawn as slivers.
constexpr int MIN_RECENT_COVER = 60;
static_assert(shelf::HEAT_DAYS <= ReadingHistory::DAYS, "activity grid reaches past the reading history");

struct Layout {
  Rect card;
  Rect recent;  // heading, covers, titles, progress and page dots
  Rect activity;
  Rect tools;
  int columns = 3;
  int featuredHeight = ShelfHome::FEATURED_COVER_HEIGHT;
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
  l.tools = homeui::capsuleToolBarRect(left, width, safe);
  // A summary line and the grid below it.
  const int activityH =
      renderer.getLineHeight(UI_10_FONT_ID) + SUMMARY_GAP + shelf::heatGridHeight(shelf::heatCell(width));
  l.activity = Rect{left, l.tools.y - SECTION_GAP - activityH, width, activityH};
  l.columns = shelf::recentColumns(width);
  const int tileW = (width - (l.columns - 1) * shelf::TILE_GAP) / l.columns;
  const int textH = 6 + renderer.getLineHeight(UI_10_FONT_ID) + 6 + renderer.getLineHeight(UI_10_FONT_ID);
  const int headingH = renderer.getLineHeight(UI_12_FONT_ID) + 10;
  // The featured and recent covers share one height, splitting the free space.
  const int freeH = l.activity.y - SECTION_GAP - top - CARD_CHROME - SECTION_GAP - headingH - textH - DOT_ROW;
  const int coverH = std::min(tileW * 3 / 2, freeH / 2);
  l.coverHeight = coverH >= MIN_RECENT_COVER ? coverH : 0;
  if (l.coverHeight > 0) l.featuredHeight = l.coverHeight;

  l.card = Rect{left, top, width, CARD_CHROME + l.featuredHeight};
  const int recentY = l.card.y + l.card.height + SECTION_GAP;
  l.recent = Rect{left, recentY, width, l.activity.y - SECTION_GAP - recentY};
  return l;
}

void drawPercentRow(const GfxRenderer& renderer, const int x, const int y, const int w, const int percent) {
  char text[8];
  snprintf(text, sizeof(text), "%d%%", percent);
  const int textW = renderer.getTextWidth(UI_10_FONT_ID, text, EpdFontFamily::BOLD);
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID);
  const int trackW = w - textW - 8;
  if (trackW > 0) {
    homeui::drawProgressTrack(renderer, Rect{x, y + (lineH - TRACK_HEIGHT) / 2, trackW, TRACK_HEIGHT}, percent, true);
  }
  renderer.drawText(UI_10_FONT_ID, x + w - textW, y, text, true, EpdFontFamily::BOLD);
}
}  // namespace

int ShelfHome::coverHeightFor(const GfxRenderer& renderer, const size_t index) {
  const Layout l = layoutFor(renderer);
  return index == 0 ? l.featuredHeight : l.coverHeight;
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

  drawCard(renderer, bookCount > 0 ? &books[0] : nullptr, l.card, l.featuredHeight, selected == 0 && bookCount > 0);
  if (bookCount > 0) targets[0] = l.card;
  if (recentTotal > 0) drawRecent(renderer, books, l.recent, l.coverHeight, selected);
  drawActivity(renderer, l.activity);
  homeui::drawCapsuleToolBar(renderer, tools, shownTools, bookCount, selected, l.tools, targets.data());
}

int ShelfHome::turnPage(const int selected, const int dir) {
  const int pages = shelf::recentPages(recentTotal, columns);
  const int next = page + dir;
  if (next < 0 || next >= pages) return -1;
  page = next;
  // A selected cover follows the page; other selections stay where they are.
  return selected >= 1 && selected <= recentTotal ? 1 + page * columns : selected;
}

void ShelfHome::drawCard(const GfxRenderer& renderer, const RecentBook* book, const Rect rect, const int coverHeight,
                         const bool selected) const {
  renderer.drawRoundedRect(rect.x, rect.y, rect.width, rect.height, selected ? 4 : 2, CARD_RADIUS, true);
  if (!book) {
    const int lineH = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, rect, UI_12_FONT_ID, rect.y + rect.height / 2 - lineH, tr(STR_NO_OPEN_BOOK),
                              true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, rect, UI_10_FONT_ID, rect.y + rect.height / 2 + 4, tr(STR_HOME_EMPTY_HINT));
    return;
  }

  const Rect cover{rect.x + CARD_PAD, rect.y + CARD_PAD, coverHeight * 2 / 3, coverHeight};
  homeui::drawCover(renderer, *book, coverHeight, cover, COVER_RADIUS);

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
    if (shelf::showReadingLine(hasDate, minutes)) {
      char duration[32];
      homeui::formatDuration(duration, sizeof(duration), static_cast<uint32_t>(minutes));
      char line[96];
      snprintf(line, sizeof(line), "%s %s", tr(STR_SHELF_READ_TODAY), duration);
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
    // The title takes up to two lines first; the author shows if it still fits.
    const int titleLines = std::clamp(space / std::max(1, titleH), 1, 2);
    const auto lines = renderer.wrappedText(UI_12_FONT_ID, book->title.c_str(), w, titleLines, EpdFontFamily::BOLD);
    int y = cover.y;
    for (const auto& line : lines) {
      renderer.drawText(UI_12_FONT_ID, x, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += titleH;
    }
    if (!book->author.empty() && y + 2 + smallH <= bottom) {
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
  // Summary line: title on the left, total time and reading days on the right.
  uint32_t total = 0;
  int readingDays = 0;
  for (int i = 0; i < shelf::HEAT_DAYS; ++i) {
    const int minutes = READING_LOG.historyMinutes(today - (shelf::HEAT_DAYS - 1) + i);
    total += minutes;
    if (minutes > 0) ++readingDays;
  }
  char duration[32];
  homeui::formatDuration(duration, sizeof(duration), total);
  char stats[96];
  snprintf(stats, sizeof(stats), "%s %s · %s %d %s", tr(STR_SHELF_TOTAL), duration, tr(STR_SHELF_READ_DAYS),
           readingDays, tr(STR_UNIT_DAYS));
  const char* title = tr(STR_SHELF_LAST_30_DAYS);
  const int titleW = renderer.getTextWidth(UI_10_FONT_ID, title, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, rect.x, rect.y, title, true, EpdFontFamily::BOLD);
  const int statsSpace = rect.width - titleW - 16;
  if (statsSpace > 40) {
    const std::string text = renderer.truncatedText(UI_10_FONT_ID, stats, statsSpace);
    renderer.drawText(UI_10_FONT_ID, rect.x + rect.width - renderer.getTextWidth(UI_10_FONT_ID, text.c_str()), rect.y,
                      text.c_str(), true);
  }

  // Grid centred under the summary.
  const int cell = shelf::heatCell(rect.width);
  const int step = cell + shelf::HEAT_GAP;
  const int gridX = rect.x + (rect.width - shelf::heatGridWidth(cell)) / 2;
  const int gridY = rect.y + rect.height - shelf::heatGridHeight(cell);
  const int radius = std::max(2, cell / 6);
  for (int i = 0; i < shelf::HEAT_DAYS; ++i) {
    const int minutes = READING_LOG.historyMinutes(today - (shelf::HEAT_DAYS - 1) + i);
    const int x = gridX + (i % shelf::HEAT_COLUMNS) * step;
    const int y = gridY + (i / shelf::HEAT_COLUMNS) * step;
    switch (shelf::heatLevel(minutes)) {
      case 0:
        renderer.drawRoundedRect(x, y, cell, cell, 1, radius, true);
        break;
      case 1:
        renderer.fillRectDither(x, y, cell, cell, Color::LightGray);
        renderer.drawRoundedRect(x, y, cell, cell, 1, radius, true);
        break;
      case 2:
        renderer.fillRectDither(x, y, cell, cell, Color::DarkGray);
        renderer.drawRoundedRect(x, y, cell, cell, 1, radius, true);
        break;
      default:
        renderer.fillRoundedRect(x, y, cell, cell, radius, Color::Black);
        break;
    }
  }
}

int ShelfHome::hitTest(const int x, const int y) const {
  for (int i = 0; i < targetCount; ++i) {
    const Rect& r = targets[i];
    if (r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}
