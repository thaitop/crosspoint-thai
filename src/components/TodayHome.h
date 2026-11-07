#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "RecentBooksStore.h"
#include "components/HomeWidgets.h"

class GfxRenderer;

// Today theme home: the last seven days of reading time as a ruled week chart
// with today in solid ink, the current book with its resume button, a short
// tally row, and the tools in a bottom bar.
// Selection indices are flat: the current book (when any), then tools.
class TodayHome {
 public:
  static constexpr int MAX_BOOKS = 1;
  static constexpr int MAX_TOOLS = 5;
  static constexpr int COVER_HEIGHT = 168;

  using Tool = homeui::Tool;

  // Reads saved progress of every recent book; touches each book's metadata
  // cache, so run it off the first paint.
  void loadProgress();
  void render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools, int toolCount,
              int selected);
  // Selectable index under a screen point, or -1.
  int hitTest(int x, int y) const;

 private:
  void drawWeek(const GfxRenderer& renderer, Rect rect, bool hasDate, int32_t today) const;
  void drawTally(const GfxRenderer& renderer, Rect rect, bool hasDate, int32_t today) const;

  int8_t currentProgress = -1;
  int8_t unfinishedBooks = -1;  // -1 until loadProgress() ran
  std::array<Rect, MAX_BOOKS + MAX_TOOLS> targets;
  int targetCount = 0;
};
