#pragma once

#include <I18nKeys.h>

#include <cstddef>
#include <cstdint>

#include "RecentBooksStore.h"
#include "components/themes/BaseTheme.h"

class GfxRenderer;

// Drawing pieces shared by the theme-drawn homes (Ledger, Today, Calendar, Shelf).
namespace homeui {

// Indexed by struct tm fields: tm_wday (0 = Sunday) and tm_mon.
inline constexpr StrId WEEKDAYS[] = {StrId::STR_WEEKDAY_SUN, StrId::STR_WEEKDAY_MON, StrId::STR_WEEKDAY_TUE,
                                     StrId::STR_WEEKDAY_WED, StrId::STR_WEEKDAY_THU, StrId::STR_WEEKDAY_FRI,
                                     StrId::STR_WEEKDAY_SAT};
inline constexpr StrId WEEKDAYS_SHORT[] = {StrId::STR_WEEKDAY_SHORT_SUN, StrId::STR_WEEKDAY_SHORT_MON,
                                           StrId::STR_WEEKDAY_SHORT_TUE, StrId::STR_WEEKDAY_SHORT_WED,
                                           StrId::STR_WEEKDAY_SHORT_THU, StrId::STR_WEEKDAY_SHORT_FRI,
                                           StrId::STR_WEEKDAY_SHORT_SAT};
inline constexpr StrId MONTHS[] = {StrId::STR_MONTH_JAN, StrId::STR_MONTH_FEB, StrId::STR_MONTH_MAR,
                                   StrId::STR_MONTH_APR, StrId::STR_MONTH_MAY, StrId::STR_MONTH_JUN,
                                   StrId::STR_MONTH_JUL, StrId::STR_MONTH_AUG, StrId::STR_MONTH_SEP,
                                   StrId::STR_MONTH_OCT, StrId::STR_MONTH_NOV, StrId::STR_MONTH_DEC};

struct Tool {
  const char* label;
  UIIcon icon;
};

// 32px bitmap of a home tool icon, or nullptr.
const uint8_t* toolIcon(UIIcon icon);
// Minutes as "45 min", "2 h" or "1 h 20 min" in the UI language.
void formatDuration(char* out, size_t size, uint32_t minutes);
// Equal cells with the icon stacked above its label; the selected cell is
// outlined. Writes each cell's hit rect to targets[firstIndex + i].
void drawToolBar(const GfxRenderer& renderer, const Tool* tools, int count, int firstIndex, int selected, Rect rect,
                 Rect* targets);
// Capsule bar of the Shelf and Calendar homes, inset from the bottom of safe.
Rect capsuleToolBarRect(int left, int width, Rect safe);
// Icons in a rounded capsule; the selected tool widens into a black pill with
// its label. Writes each cell's hit rect to targets[firstIndex + i].
void drawCapsuleToolBar(const GfxRenderer& renderer, const Tool* tools, int count, int firstIndex, int selected,
                        Rect rect, Rect* targets);
// Outlined bar filled to percent (0-100).
void drawProgressTrack(const GfxRenderer& renderer, Rect rect, int percent);
// Cover thumb generated at thumbHeight, filled into slot; placeholder when
// missing. A radius rounds the corners and the border; the cut corners are
// cleared to white, so draw it on a white background.
void drawCover(const GfxRenderer& renderer, const RecentBook& book, int thumbHeight, Rect slot, int radius = 0);
// Cover with title, author, progress (percent -1 = unknown) and a resume
// button that is filled when selected. Without percentText the bar alone
// carries the progress, directly under the title and author.
void drawFeaturedBook(const GfxRenderer& renderer, const RecentBook& book, int percent, int thumbHeight, Rect rect,
                      bool selected, bool percentText = true);

}  // namespace homeui
