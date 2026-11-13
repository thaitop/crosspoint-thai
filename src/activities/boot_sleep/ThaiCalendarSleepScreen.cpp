#include "ThaiCalendarSleepScreen.h"

#ifndef OMIT_FONTS

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "components/HomeWidgets.h"
#include "fontIds.h"
#include "util/ThaiCalendar.h"

namespace {

constexpr int BUDDHIST_ERA_OFFSET = 543;
constexpr int SECTION_COUNT = 5;  // header, day, moon, week, important day

constexpr StrId MONTHS_SHORT[] = {StrId::STR_MONTH_SHORT_JAN, StrId::STR_MONTH_SHORT_FEB, StrId::STR_MONTH_SHORT_MAR,
                                  StrId::STR_MONTH_SHORT_APR, StrId::STR_MONTH_SHORT_MAY, StrId::STR_MONTH_SHORT_JUN,
                                  StrId::STR_MONTH_SHORT_JUL, StrId::STR_MONTH_SHORT_AUG, StrId::STR_MONTH_SHORT_SEP,
                                  StrId::STR_MONTH_SHORT_OCT, StrId::STR_MONTH_SHORT_NOV, StrId::STR_MONTH_SHORT_DEC};
constexpr StrId ZODIAC[] = {StrId::STR_ZODIAC_RAT,     StrId::STR_ZODIAC_OX,     StrId::STR_ZODIAC_TIGER,
                            StrId::STR_ZODIAC_RABBIT,  StrId::STR_ZODIAC_DRAGON, StrId::STR_ZODIAC_SNAKE,
                            StrId::STR_ZODIAC_HORSE,   StrId::STR_ZODIAC_GOAT,   StrId::STR_ZODIAC_MONKEY,
                            StrId::STR_ZODIAC_ROOSTER, StrId::STR_ZODIAC_DOG,    StrId::STR_ZODIAC_PIG};
// Same order as thai_cal::Holiday.
constexpr StrId HOLIDAYS[] = {StrId::STR_THAI_HOLIDAY_NEW_YEAR,      StrId::STR_THAI_HOLIDAY_CHILDRENS_DAY,
                              StrId::STR_THAI_HOLIDAY_CHAKRI_DAY,    StrId::STR_THAI_HOLIDAY_SONGKRAN,
                              StrId::STR_THAI_HOLIDAY_LABOUR_DAY,    StrId::STR_THAI_HOLIDAY_MOTHERS_DAY,
                              StrId::STR_THAI_HOLIDAY_FATHERS_DAY,   StrId::STR_THAI_HOLIDAY_CONSTITUTION_DAY,
                              StrId::STR_THAI_HOLIDAY_NEW_YEARS_EVE, StrId::STR_THAI_HOLIDAY_MAKHA_BUCHA,
                              StrId::STR_THAI_HOLIDAY_VISAKHA_BUCHA, StrId::STR_THAI_HOLIDAY_ASALHA_BUCHA,
                              StrId::STR_THAI_HOLIDAY_KHAO_PHANSA,   StrId::STR_THAI_HOLIDAY_OK_PHANSA,
                              StrId::STR_THAI_HOLIDAY_LOY_KRATHONG};
static_assert(sizeof(HOLIDAYS) / sizeof(HOLIDAYS[0]) == static_cast<size_t>(thai_cal::Holiday::COUNT));

// Thai shows the Buddhist Era and plain numbers; every other language falls back
// to English, which uses the Gregorian year and ordinals ("4th"). The %s after the
// night in STR_THAI_CAL_WAXING/WANING is that English suffix, so another language
// needs a change here before it translates those keys.
bool thaiText() { return I18N.getLanguage() == Language::TH; }

const char* ordinalSuffix(const int n) {
  if (thaiText()) return "";
  if (n % 100 >= 11 && n % 100 <= 13) return "th";
  switch (n % 10) {
    case 1:
      return "st";
    case 2:
      return "nd";
    case 3:
      return "rd";
    default:
      return "th";
  }
}

void formatLunar(const thai_cal::LunarDate& lunar, char* buf, const size_t len) {
  char second[32] = "";
  if (lunar.secondEighth) snprintf(second, sizeof(second), " %s", tr(STR_THAI_CAL_SECOND_EIGHTH));
  snprintf(buf, len, lunar.waxing ? tr(STR_THAI_CAL_WAXING) : tr(STR_THAI_CAL_WANING), lunar.night,
           ordinalSuffix(lunar.night), lunar.month, second);
}

struct Layout {
  int margin;
  int width;
  int headerH;
  int dayH;
  int dayScale256;  // DIGITS_LARGE is drawn scaled up to dayH
  int moonR;
  int moonH;
  int weekH;
  int eventH;
};

void drawMoon(const GfxRenderer& renderer, const int cx, const int cy, const int radius,
              const thai_cal::LunarDate& lunar) {
  // Black disc, then the lit part in white inside a 2 px ring.
  constexpr int RING = 2;
  const uint16_t lit = thai_cal::moonLitPermille(lunar);
  for (int dy = -radius; dy <= radius; ++dy) {
    int x0, x1;
    if (thai_cal::moonRowSpan(radius, dy, 1000, true, x0, x1)) renderer.fillRect(cx + x0, cy + dy, x1 - x0 + 1, 1);
    if (thai_cal::moonRowSpan(radius - RING, dy, lit, lunar.waxing, x0, x1)) {
      renderer.fillRect(cx + x0, cy + dy, x1 - x0 + 1, 1, false);
    }
  }
}

void drawDayNumber(const GfxRenderer& renderer, const int screenW, const int top, const Layout& l, const int day) {
  char digits[3];
  snprintf(digits, sizeof(digits), "%d", day);
  int width = 0;
  for (const char* c = digits; *c; ++c) {
    int32_t advance = 0;
    int glyphTop = 0;
    renderer.getCodepointMetrics(DIGITS_LARGE_FONT_ID, *c, EpdFontFamily::REGULAR, advance, glyphTop);
    width += fp4::toPixel(advance * l.dayScale256 / 256);
  }
  int x = (screenW - width) / 2;
  for (const char* c = digits; *c; ++c) {
    x += renderer.drawScaledCodepoint(DIGITS_LARGE_FONT_ID, *c, EpdFontFamily::REGULAR, x, top + l.dayH, l.dayScale256);
  }
}

void drawWeek(const GfxRenderer& renderer, const Layout& l, const int top, const int32_t today) {
  const int boxW = l.width / 7;
  const int left = l.margin + (l.width - boxW * 7) / 2;
  const int line10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int32_t sunday = today - thai_cal::weekday(today);
  for (int i = 0; i < 7; ++i) {
    const int x = left + i * boxW;
    const bool isToday = sunday + i == today;
    if (isToday) {
      renderer.fillRect(x, top, boxW, l.weekH);
    } else {
      renderer.drawRect(x, top, boxW, l.weekH);
    }
    int y, m, d;
    thai_cal::civilFromDays(sunday + i, y, m, d);
    char num[4];
    snprintf(num, sizeof(num), "%d", d);
    const char* name = I18N.get(homeui::WEEKDAYS_SHORT[i]);
    const int pad = (l.weekH - line10 - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, x + (boxW - renderer.getTextWidth(UI_10_FONT_ID, name)) / 2, top + pad, name,
                      !isToday);
    const auto style = isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(UI_12_FONT_ID, x + (boxW - renderer.getTextWidth(UI_12_FONT_ID, num, style)) / 2,
                      top + pad + line10, num, !isToday, style);
  }
}

void drawImportantDay(const GfxRenderer& renderer, const Layout& l, int y, const int32_t today) {
  thai_cal::ImportantDay next{};
  if (!thai_cal::nextImportantDay(today, next)) return;
  renderer.fillRect(l.margin, y, l.width, 2);
  y += renderer.getLineHeight(SMALL_FONT_ID) / 2;
  renderer.drawCenteredText(SMALL_FONT_ID, y, tr(STR_THAI_CAL_UPCOMING));
  y += renderer.getLineHeight(SMALL_FONT_ID);
  renderer.drawCenteredText(UI_12_FONT_ID, y, I18N.get(HOLIDAYS[static_cast<int>(next.holiday)]), true,
                            EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID);
  char when[64];
  if (next.day == today) {
    snprintf(when, sizeof(when), "%s", tr(STR_THAI_CAL_TODAY));
  } else {
    int year, month, day;
    thai_cal::civilFromDays(next.day, year, month, day);
    const int daysLeft = static_cast<int>(next.day - today);
    const char* monthName = I18N.get(MONTHS_SHORT[month - 1]);
    if (daysLeft == 1) {
      snprintf(when, sizeof(when), tr(STR_THAI_CAL_TOMORROW), day, monthName);
    } else {
      snprintf(when, sizeof(when), tr(STR_THAI_CAL_IN_DAYS), day, monthName, daysLeft);
    }
  }
  renderer.drawCenteredText(UI_10_FONT_ID, y, when);
}

}  // namespace

void drawThaiCalendarSleepScreen(const GfxRenderer& renderer, const struct tm& date) {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int32_t today = thai_cal::daysFromCivil(date.tm_year + 1900, date.tm_mon + 1, date.tm_mday);
  thai_cal::LunarDate lunar{};
  const bool hasLunar = thai_cal::lunarDate(today, lunar);

  Layout l{};
  l.margin = screenH / 32;
  l.width = screenW - 2 * l.margin;
  const int line10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int line12 = renderer.getLineHeight(UI_12_FONT_ID);
  const int lineSmall = renderer.getLineHeight(SMALL_FONT_ID);
  l.headerH = line12 + line10;
  int32_t advance = 0;
  int digitTop = 0;
  renderer.getCodepointMetrics(DIGITS_LARGE_FONT_ID, '0', EpdFontFamily::REGULAR, advance, digitTop);
  // Sizes follow the approved 480x800 mockup; the moon uses the short side so
  // landscape screens do not overflow.
  l.moonR = std::min(screenW, screenH * 3 / 5) * 56 / 480;
  l.moonH = hasLunar ? 2 * l.moonR + line10 / 2 + line12 : 0;
  l.weekH = line10 + line12 + line10 / 2;
  l.eventH = lineSmall / 2 + lineSmall + line12 + line10;
  const int gaps = hasLunar ? SECTION_COUNT - 1 : SECTION_COUNT - 2;
  const int minGap = l.margin / 2;
  const int room = screenH - 2 * l.margin - l.headerH - l.moonH - l.weekH - l.eventH - gaps * minGap;
  // Digits are 8/25 of the screen height, smaller when the rest needs the room.
  l.dayH = std::max(0, std::min(screenH * 8 / 25, room));
  l.dayScale256 = digitTop > 0 ? l.dayH * 256 / digitTop : 256;

  // Spread the sections over the full height with equal gaps.
  const int content = l.headerH + l.dayH + l.moonH + l.weekH + l.eventH;
  const int gap = (screenH - 2 * l.margin - content) / gaps;
  int y = l.margin;

  char text[96];
  const int year = date.tm_year + 1900;
  snprintf(text, sizeof(text), tr(STR_THAI_CAL_YEAR), thaiText() ? year + BUDDHIST_ERA_OFFSET : year,
           I18N.get(ZODIAC[thai_cal::zodiacIndex(year)]));
  renderer.drawCenteredText(UI_12_FONT_ID, y, text);
  snprintf(text, sizeof(text), "%s  %s", I18N.get(homeui::MONTHS[date.tm_mon]),
           I18N.get(homeui::WEEKDAYS[date.tm_wday]));
  renderer.drawCenteredText(UI_10_FONT_ID, y + line12, text);
  y += l.headerH + gap;

  drawDayNumber(renderer, screenW, y, l, date.tm_mday);
  y += l.dayH + gap;

  if (hasLunar) {
    drawMoon(renderer, screenW / 2, y + l.moonR, l.moonR, lunar);
    formatLunar(lunar, text, sizeof(text));
    // English month-8 lines are long; narrow screens get the smaller face.
    const int lunarFont =
        renderer.getTextWidth(UI_12_FONT_ID, text, EpdFontFamily::BOLD) <= l.width ? UI_12_FONT_ID : UI_10_FONT_ID;
    renderer.drawCenteredText(lunarFont, y + 2 * l.moonR + line10 / 2, text, true, EpdFontFamily::BOLD);
    y += l.moonH + gap;
  }

  drawWeek(renderer, l, y, today);
  drawImportantDay(renderer, l, screenH - l.margin - l.eventH, today);
}

#endif  // OMIT_FONTS
