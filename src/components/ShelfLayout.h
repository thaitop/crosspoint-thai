#pragma once

#include <algorithm>

// Layout rules of the Shelf home, kept free of the renderer so host tests can
// check them.
namespace shelf {

// Recent books after the featured one, shown a page at a time.
constexpr int MAX_RECENT = 9;
constexpr int MIN_TILE_WIDTH = 110;
constexpr int TILE_GAP = 16;

// Activity grid: the last HEAT_DAYS days in rows of HEAT_COLUMNS, oldest
// first, today last, spread across the box width.
constexpr int HEAT_DAYS = 30;
constexpr int HEAT_COLUMNS = 15;
constexpr int HEAT_ROWS = (HEAT_DAYS + HEAT_COLUMNS - 1) / HEAT_COLUMNS;
constexpr int HEAT_GAP = 5;
constexpr int HEAT_MAX_CELL = 28;

// Covers per page in a row this wide: 3, or 2 on narrow screens.
constexpr int recentColumns(const int width) { return 3 * MIN_TILE_WIDTH + 2 * TILE_GAP <= width ? 3 : 2; }

// Recent books for bookCount loaded books; the first one is featured.
constexpr int recentCount(const int bookCount) { return std::clamp(bookCount - 1, 0, MAX_RECENT); }

constexpr int recentPages(const int count, const int columns) { return (count + columns - 1) / columns; }

// Shade of a day in the activity grid, with the Calendar theme's steps:
// 0 none, 1 under an hour, 2 under two hours, 3 two hours or more.
constexpr int heatLevel(const int minutes) { return minutes <= 0 ? 0 : minutes < 60 ? 1 : minutes < 120 ? 2 : 3; }

// Largest square cell that fits HEAT_COLUMNS across width.
constexpr int heatCell(const int width) {
  return std::clamp((width - (HEAT_COLUMNS - 1) * HEAT_GAP) / HEAT_COLUMNS, 1, HEAT_MAX_CELL);
}
constexpr int heatGridWidth(const int cell) { return HEAT_COLUMNS * cell + (HEAT_COLUMNS - 1) * HEAT_GAP; }
constexpr int heatGridHeight(const int cell) { return HEAT_ROWS * cell + (HEAT_ROWS - 1) * HEAT_GAP; }

// The reading-time line needs a local date and something to report.
constexpr bool showReadingLine(const bool hasDate, const int minutesToday, const int streakDays) {
  return hasDate && (minutesToday > 0 || streakDays > 0);
}

}  // namespace shelf
