#include "TodayHome.h"

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

namespace {
constexpr int TOOL_BAR_HEIGHT = 80;
constexpr int SCALE_GUTTER = 32;
constexpr int RADIUS = 6;
#ifndef OMIT_FONTS
constexpr int NUMBER_FONT_ID = NOTOSANS_18_FONT_ID;
#else
constexpr int NUMBER_FONT_ID = UI_12_FONT_ID;
#endif

using homeui::formatDuration;
using homeui::MONTHS;
using homeui::WEEKDAYS;
using homeui::WEEKDAYS_SHORT;
// Rule spacing in minutes; the smallest that keeps the chart to four rules wins.
constexpr int RULE_STEPS[] = {15, 30, 60, 120, 240, 480};
constexpr const char* NO_VALUE = "-";

int minutesOn(const int32_t day) { return static_cast<int>((READING_LOG.secondsOn(day) + 30) / 60); }
}  // namespace

void TodayHome::loadProgress() {
  int unfinished = 0;
  bool first = true;
  for (const auto& book : RECENT_BOOKS.getBooks()) {
    if (RecentBooksStore::isMissing(book)) continue;
    const int percent = loadBookProgress(book.path);
    if (first) currentProgress = static_cast<int8_t>(percent);
    first = false;
    if (percent < 100) ++unfinished;
  }
  unfinishedBooks = static_cast<int8_t>(unfinished);
}

void TodayHome::render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools,
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
  const int32_t today = hasDate ? ReadingLogStore::dayNumber(date) : -1;

  const int left = safe.x + metrics.contentSidePadding;
  const int width = safe.width - 2 * metrics.contentSidePadding;
  int y = std::max(safe.y, metrics.topPadding + metrics.batteryBarHeight) + 8;
  char title[96];
  if (hasDate) {
    snprintf(title, sizeof(title), "%s %d %s", I18N.get(WEEKDAYS[date.tm_wday]), date.tm_mday,
             I18N.get(MONTHS[date.tm_mon]));
  } else {
    snprintf(title, sizeof(title), "%s", tr(STR_TODAY_TITLE));
  }
  renderer.drawText(UI_12_FONT_ID, left, y, title, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;

  const Rect toolBar{safe.x, safe.y + safe.height - TOOL_BAR_HEIGHT, safe.width, TOOL_BAR_HEIGHT};
  homeui::drawToolBar(renderer, tools, shownTools, bookCount, selected, toolBar, targets.data());

  const int bodyBottom = toolBar.y - 12;
  const int tallyHeight = renderer.getLineHeight(UI_10_FONT_ID) + renderer.getLineHeight(UI_12_FONT_ID) + 18;
  Rect week;
  Rect book;
  Rect tally;
  if (landscape) {
    const int half = (width - 32) / 2;
    const int rightX = left + half + 32;
    const int rightW = width - half - 32;
    week = Rect{left, y, half, bodyBottom - y};
    tally = Rect{rightX, bodyBottom - tallyHeight, rightW, tallyHeight};
    book = Rect{rightX, y, rightW, std::min(COVER_HEIGHT, tally.y - 16 - y)};
  } else {
    tally = Rect{left, bodyBottom - tallyHeight, width, tallyHeight};
    book = Rect{left, tally.y - 16 - COVER_HEIGHT, width, COVER_HEIGHT};
    week = Rect{left, y, width, book.y - 28 - y};
  }

  drawWeek(renderer, week, hasDate, today);

  if (bookCount > 0 && book.height > 0) {
    homeui::drawFeaturedBook(renderer, books[0], currentProgress, COVER_HEIGHT, book, selected == 0);
    targets[0] = book;
  } else if (book.height > 0) {
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    UITheme::drawCenteredText(renderer, book, UI_12_FONT_ID, book.y + book.height / 2 - lineHeight,
                              tr(STR_NO_OPEN_BOOK), true, EpdFontFamily::BOLD);
    UITheme::drawCenteredText(renderer, book, UI_10_FONT_ID, book.y + book.height / 2 + 4, tr(STR_HOME_EMPTY_HINT));
  }

  drawTally(renderer, tally, hasDate, today);
}

void TodayHome::drawWeek(const GfxRenderer& renderer, const Rect rect, const bool hasDate, const int32_t today) const {
  const int smallHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int numberHeight = renderer.getLineHeight(NUMBER_FONT_ID);
  const int labelBand = smallHeight + 12;
  const int valueBand = numberHeight + smallHeight + 8;
  const Rect plot{rect.x + SCALE_GUTTER, rect.y + valueBand, rect.width - SCALE_GUTTER,
                  rect.height - valueBand - labelBand};
  if (plot.height < 24 || plot.width < 7 * 8) return;
  const int baseline = plot.y + plot.height;
  // Axis title in the headroom kept above the plot for today's value.
  renderer.drawText(UI_10_FONT_ID, rect.x, rect.y, tr(STR_TODAY_CHART_TITLE), true);

  if (!hasDate) {
    renderer.fillRect(plot.x, baseline, plot.width, 2, true);
    UITheme::drawCenteredWrappedText(renderer, plot, UI_12_FONT_ID, tr(STR_TODAY_CLOCK_HINT), 3, true,
                                     EpdFontFamily::BOLD);
    return;
  }

  int minutes[ReadingLogStore::DAYS];
  int most = 0;
  for (int i = 0; i < ReadingLogStore::DAYS; ++i) {
    minutes[i] = minutesOn(today - (ReadingLogStore::DAYS - 1) + i);
    most = std::max(most, minutes[i]);
  }
  int step = RULE_STEPS[std::size(RULE_STEPS) - 1];
  for (const int candidate : RULE_STEPS) {
    if (most <= 4 * candidate) {
      step = candidate;
      break;
    }
  }
  const int scale = std::max(2, (most + step - 1) / step) * step;

  // Dotted rules like a ruled notebook page, labelled in minutes in the gutter.
  const int smallFontHeight = renderer.getLineHeight(SMALL_FONT_ID);
  for (int value = step; value <= scale; value += step) {
    const int ruleY = baseline - plot.height * value / scale;
    for (int x = plot.x; x < plot.x + plot.width; x += 4) renderer.drawPixel(x, ruleY, true);
    char label[8];
    snprintf(label, sizeof(label), "%d", value);
    const int labelW = renderer.getTextWidth(SMALL_FONT_ID, label);
    renderer.drawText(SMALL_FONT_ID, plot.x - 6 - labelW, ruleY - smallFontHeight / 2, label, true);
  }
  renderer.fillRect(plot.x, baseline, plot.width, 2, true);

  // Past days recede as dithered bars; today is solid ink with its value on top.
  const int colW = plot.width / ReadingLogStore::DAYS;
  const int barW = std::max(8, colW * 5 / 9);
  for (int i = 0; i < ReadingLogStore::DAYS; ++i) {
    const bool isToday = i == ReadingLogStore::DAYS - 1;
    const int cx = plot.x + colW * i + colW / 2;
    const int barX = cx - barW / 2;
    const int h = minutes[i] > 0 ? std::max(3, plot.height * minutes[i] / scale) : 0;
    if (h > 0 && isToday) {
      renderer.fillRect(barX, baseline - h, barW, h, true);
    } else if (h > 0) {
      renderer.fillRectDither(barX, baseline - h, barW, h, Color::LightGray);
      renderer.drawRect(barX, baseline - h, barW, h, true);
    }

    const char* day = I18N.get(WEEKDAYS_SHORT[ReadingLogStore::weekday(today - (ReadingLogStore::DAYS - 1) + i)]);
    const int labelY = baseline + 2 + 8;
    if (isToday) {
      const int textW = renderer.getTextWidth(UI_10_FONT_ID, day, EpdFontFamily::BOLD);
      const int pillW = textW + 16;
      renderer.fillRoundedRect(cx - pillW / 2, labelY - 3, pillW, smallHeight + 6, RADIUS, Color::Black);
      renderer.drawText(UI_10_FONT_ID, cx - textW / 2, labelY, day, false, EpdFontFamily::BOLD);
    } else {
      renderer.drawText(UI_10_FONT_ID, cx - renderer.getTextWidth(UI_10_FONT_ID, day) / 2, labelY, day, true);
    }

    if (isToday) {
      char value[8];
      snprintf(value, sizeof(value), "%d", minutes[i]);
      const char* unit = tr(STR_UNIT_MINUTES);
      const int valueW = renderer.getTextWidth(NUMBER_FONT_ID, value, EpdFontFamily::BOLD);
      const int unitW = renderer.getTextWidth(UI_10_FONT_ID, unit);
      // Centered over the bar, but kept inside the chart at the right edge.
      const int rightEdge = rect.x + rect.width;
      const int unitY = baseline - h - 6 - smallHeight;
      renderer.drawText(NUMBER_FONT_ID, std::min(cx - valueW / 2, rightEdge - valueW), unitY - numberHeight, value,
                        true, EpdFontFamily::BOLD);
      renderer.drawText(UI_10_FONT_ID, std::min(cx - unitW / 2, rightEdge - unitW), unitY, unit, true);
    }
  }
}

void TodayHome::drawTally(const GfxRenderer& renderer, const Rect rect, const bool hasDate, const int32_t today) const {
  char week[40];
  char streak[24];
  char unfinished[24];
  if (hasDate) {
    uint32_t total = 0;
    for (int i = 0; i < ReadingLogStore::DAYS; ++i) total += minutesOn(today - i);
    formatDuration(week, sizeof(week), total);
    snprintf(streak, sizeof(streak), "%d %s", READING_LOG.streak(today), tr(STR_UNIT_DAYS));
  } else {
    snprintf(week, sizeof(week), "%s", NO_VALUE);
    snprintf(streak, sizeof(streak), "%s", NO_VALUE);
  }
  if (unfinishedBooks >= 0) {
    snprintf(unfinished, sizeof(unfinished), "%d %s", unfinishedBooks, tr(STR_UNIT_BOOKS));
  } else {
    snprintf(unfinished, sizeof(unfinished), "%s", NO_VALUE);
  }

  struct Cell {
    const char* label;
    const char* value;
  };
  const Cell cells[] = {
      {tr(STR_TODAY_LAST_7_DAYS), week}, {tr(STR_TODAY_STREAK), streak}, {tr(STR_HOME_IN_PROGRESS), unfinished}};
  // The week total is the longest value, so its cell gets the extra width.
  const int firstW = rect.width * 2 / 5;
  const int otherW = (rect.width - firstW) / 2;

  renderer.fillRect(rect.x, rect.y, rect.width, 2, true);
  const int labelY = rect.y + 10;
  const int valueY = labelY + renderer.getLineHeight(UI_10_FONT_ID) + 2;
  int x = rect.x;
  for (int i = 0; i < 3; ++i) {
    const int cellW = i == 0 ? firstW : (i == 2 ? rect.x + rect.width - x : otherW);
    const int pad = i == 0 ? 0 : 12;
    if (i > 0) renderer.drawLine(x, rect.y + 10, x, rect.y + rect.height - 4, true);
    const int textW = cellW - pad - 8;
    renderer.drawText(UI_10_FONT_ID, x + pad, labelY,
                      renderer.truncatedText(UI_10_FONT_ID, cells[i].label, textW).c_str(), true);
    renderer.drawText(UI_12_FONT_ID, x + pad, valueY,
                      renderer.truncatedText(UI_12_FONT_ID, cells[i].value, textW, EpdFontFamily::BOLD).c_str(), true,
                      EpdFontFamily::BOLD);
    x += cellW;
  }
}

int TodayHome::hitTest(const int x, const int y) const {
  for (int i = 0; i < targetCount; ++i) {
    const Rect& r = targets[i];
    if (r.width > 0 && x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return i;
  }
  return -1;
}
