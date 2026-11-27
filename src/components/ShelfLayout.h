#pragma once

#include <algorithm>

// Layout rules of the Shelf home, kept free of the renderer so host tests can
// check them.
namespace shelf {

// Recent books after the featured one, shown a page at a time.
constexpr int MAX_RECENT = 9;
constexpr int MIN_TILE_WIDTH = 110;
constexpr int TILE_GAP = 16;

// Activity grid: one column per week, one row per weekday.
constexpr int HEAT_WEEKS = 13;
constexpr int HEAT_CELL = 12;
constexpr int HEAT_GAP = 3;

// Covers per page in a row this wide: 3, or 2 on narrow screens.
constexpr int recentColumns(const int width) { return 3 * MIN_TILE_WIDTH + 2 * TILE_GAP <= width ? 3 : 2; }

// Recent books for bookCount loaded books; the first one is featured.
constexpr int recentCount(const int bookCount) { return std::clamp(bookCount - 1, 0, MAX_RECENT); }

constexpr int recentPages(const int count, const int columns) { return (count + columns - 1) / columns; }

// Shade of a day in the activity grid, with the Calendar theme's steps:
// 0 none, 1 under an hour, 2 under two hours, 3 two hours or more.
constexpr int heatLevel(const int minutes) { return minutes <= 0 ? 0 : minutes < 60 ? 1 : minutes < 120 ? 2 : 3; }

constexpr int heatGridWidth() { return HEAT_WEEKS * HEAT_CELL + (HEAT_WEEKS - 1) * HEAT_GAP; }
constexpr int heatGridHeight() { return 7 * HEAT_CELL + 6 * HEAT_GAP; }

// The reading-time line needs a local date and something to report.
constexpr bool showReadingLine(const bool hasDate, const int minutesToday, const int streakDays) {
  return hasDate && (minutesToday > 0 || streakDays > 0);
}

}  // namespace shelf
