#include "LedgerHome.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "HomeWidgets.h"
#include "UITheme.h"
#include "fontIds.h"
#include "util/BookProgress.h"

namespace {
constexpr int TOOL_BAR_HEIGHT = 80;
constexpr int RADIUS = 6;
constexpr int SELECTION_LINE = 2;
constexpr int ROW_MAX_HEIGHT = 72;
constexpr int ROW_MIN_HEIGHT = 60;
constexpr int ROW_RIGHT_WIDTH = 84;
}  // namespace

void LedgerHome::loadProgress(const std::vector<RecentBook>& books) {
  for (size_t i = 0; i < progress.size(); ++i) {
    progress[i] = i < books.size() ? static_cast<int8_t>(loadBookProgress(books[i].path)) : -1;
  }
}

void LedgerHome::render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools,
                        const int toolCount, const int selected) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true);
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  const int bookCount = std::min(static_cast<int>(books.size()), MAX_BOOKS);
  targets.fill(Rect{});
  targetCount = bookCount + std::min(toolCount, MAX_TOOLS);
  rowCount = std::max(0, bookCount - 1);
  visibleRows = 0;

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, renderer.getScreenWidth(), metrics.batteryBarHeight}, nullptr,
                 nullptr, false);

  const int left = safe.x + metrics.contentSidePadding;
  const int width = safe.width - 2 * metrics.contentSidePadding;
  int y = std::max(safe.y, metrics.topPadding + metrics.batteryBarHeight) + 8;
  renderer.drawText(UI_12_FONT_ID, left, y, tr(STR_HOME_SHELF), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 12;

  const Rect toolBar{safe.x, safe.y + safe.height - TOOL_BAR_HEIGHT, safe.width, TOOL_BAR_HEIGHT};
  homeui::drawToolBar(renderer, tools, std::min(toolCount, MAX_TOOLS), bookCount, selected, toolBar, targets.data());

  const int bodyBottom = toolBar.y - 8;
  if (bookCount == 0) {
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    const Rect body{left, y, width, bodyBottom - y};
    UITheme::drawCenteredText(renderer, body, UI_12_FONT_ID, body.y + body.height / 2 - lineHeight,
                              tr(STR_NO_OPEN_BOOK), true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, body, UI_10_FONT_ID, body.y + body.height / 2 + 4, tr(STR_HOME_EMPTY_HINT));
    return;
  }

  Rect featured;
  Rect list;
  if (landscape) {
    const int half = (width - 24) / 2;
    featured = Rect{left, y, half, bodyBottom - y};
    list = Rect{left + half + 24, y, width - half - 24, bodyBottom - y};
  } else {
    featured = Rect{left, y, width, std::min(FEATURED_COVER_HEIGHT, bodyBottom - y)};
    list = Rect{left, featured.y + featured.height + 20, width, bodyBottom - featured.y - featured.height - 20};
  }
  homeui::drawFeaturedBook(renderer, books[0], progress[0], FEATURED_COVER_HEIGHT, featured, selected == 0);
  targets[0] = featured;

  if (bookCount < 2 || list.height <= 0) return;
  // Section rule and heading, then rows that shrink to fit the remaining band;
  // past the minimum height the list scrolls with the selection instead.
  renderer.fillRect(list.x, list.y, list.width, 2, true);
  const int headingY = list.y + 8;
  renderer.drawText(UI_12_FONT_ID, list.x, headingY, tr(STR_HOME_IN_PROGRESS), true, EpdFontFamily::BOLD);
  const int rowsTop = headingY + renderer.getLineHeight(UI_12_FONT_ID) + 6;
  const int rowsHeight = list.y + list.height - rowsTop;
  int rowHeight = std::min(ROW_MAX_HEIGHT, rowsHeight / rowCount);
  visibleRows = rowCount;
  if (rowHeight < ROW_MIN_HEIGHT) {
    rowHeight = ROW_MAX_HEIGHT;
    visibleRows = rowsHeight / rowHeight;
  }
  if (visibleRows <= 0) return;

  const int lastFirstRow = bookCount - visibleRows;
  if (selected >= 1 && selected < bookCount) {
    if (selected < firstRow) firstRow = selected;
    if (selected >= firstRow + visibleRows) firstRow = selected - visibleRows + 1;
  }
  firstRow = std::clamp(firstRow, 1, lastFirstRow);
  if (visibleRows < rowCount) {
    char range[16];
    snprintf(range, sizeof(range), "%d-%d / %d", firstRow, firstRow + visibleRows - 1, rowCount);
    const int rangeW = renderer.getTextWidth(UI_10_FONT_ID, range);
    const int rangeY = headingY + renderer.getLineHeight(UI_12_FONT_ID) - renderer.getLineHeight(UI_10_FONT_ID);
    renderer.drawText(UI_10_FONT_ID, list.x + list.width - rangeW, rangeY, range, true);
  }

  for (int i = firstRow; i < firstRow + visibleRows; ++i) {
    const Rect row{list.x, rowsTop + (i - firstRow) * rowHeight, list.width, rowHeight};
    if (i > firstRow) renderer.drawLine(row.x, row.y, row.x + row.width - 1, row.y, true);
    drawRow(renderer, books[i], progress[i], row, selected == i);
    targets[i] = row;
  }
}

void LedgerHome::drawRow(const GfxRenderer& renderer, const RecentBook& book, const int percent, const Rect rect,
                         const bool selected) const {
  constexpr int PAD = 6;
  const int coverHeight = std::min(ROW_COVER_HEIGHT, rect.height - 2 * PAD);
  const int coverWidth = coverHeight * 2 / 3;
  const int coverY = rect.y + (rect.height - coverHeight) / 2;
  if (coverHeight > 0) {
    homeui::drawCover(renderer, book, ROW_COVER_HEIGHT, Rect{rect.x + PAD, coverY, coverWidth, coverHeight});
  }

  const int titleHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int textX = rect.x + PAD + coverWidth + 14;
  const int rightX = rect.x + rect.width - PAD - ROW_RIGHT_WIDTH;
  const int textW = std::max(0, rightX - 12 - textX);
  const bool hasAuthor = !book.author.empty();
  int y = rect.y + (rect.height - titleHeight - (hasAuthor ? smallHeight : 0)) / 2;
  renderer.drawText(UI_12_FONT_ID, textX, y,
                    renderer.truncatedText(UI_12_FONT_ID, book.title.c_str(), textW, EpdFontFamily::BOLD).c_str(), true,
                    EpdFontFamily::BOLD);
  if (hasAuthor) {
    renderer.drawText(UI_10_FONT_ID, textX, y + titleHeight,
                      renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), textW).c_str(), true);
  }

  const char* status = nullptr;
  char text[8];
  if (percent >= 100) {
    status = tr(STR_HOME_FINISHED);
  } else if (percent >= 0) {
    snprintf(text, sizeof(text), "%d%%", percent);
    status = text;
  }
  if (status) {
    const int statusY = rect.y + (rect.height - titleHeight - 10) / 2;
    const int statusW = renderer.getTextWidth(UI_12_FONT_ID, status, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, rightX + ROW_RIGHT_WIDTH - statusW, statusY, status, true, EpdFontFamily::BOLD);
    homeui::drawProgressTrack(renderer, Rect{rightX, statusY + titleHeight + 4, ROW_RIGHT_WIDTH, 6}, percent);
  }

  if (selected) renderer.drawRoundedRect(rect.x, rect.y + 2, rect.width, rect.height - 4, SELECTION_LINE, RADIUS, true);
}

int LedgerHome::pageList(const int selected, const int dir) {
  // Tools keep stepping; the featured book swipes forward into the list.
  if (visibleRows <= 0 || visibleRows >= rowCount || selected > rowCount || (selected == 0 && dir < 0)) return -1;
  const int lastFirstRow = rowCount - visibleRows + 1;
  if (dir > 0) {
    if (selected == 0) return firstRow;
    if (firstRow == lastFirstRow) return rowCount;
    firstRow = std::min(firstRow + visibleRows, lastFirstRow);
  } else {
    if (firstRow == 1) return 0;
    firstRow = std::max(firstRow - visibleRows, 1);
  }
  return firstRow;
}

int LedgerHome::hitTest(const int x, const int y) const {
  for (int i = 0; i < targetCount; ++i) {
    const Rect& r = targets[i];
    if (r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}
