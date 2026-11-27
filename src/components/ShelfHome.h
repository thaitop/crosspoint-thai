#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "RecentBooksStore.h"
#include "components/HomeWidgets.h"
#include "components/ShelfLayout.h"

class GfxRenderer;

// Shelf theme home: the current book in a rounded card with its progress,
// today's reading time and streak, and a resume capsule; the recent books as
// a row of covers turned a page at a time; the last 13 weeks of reading as an
// activity grid; and the tools in a capsule bar at the bottom.
// Selection indices are flat: the current book, recent books, then tools.
class ShelfHome {
 public:
  static constexpr int MAX_BOOKS = 1 + shelf::MAX_RECENT;
  static constexpr int MAX_TOOLS = 5;
  static constexpr int FEATURED_COVER_HEIGHT = 140;

  using Tool = homeui::Tool;

  // Covers are drawn from thumbs generated at exactly this height; recent
  // covers take the height this screen leaves for them.
  static int coverHeightFor(const GfxRenderer& renderer, size_t index);
  // Books this screen shows: the current book plus the recent row, if it fits.
  static int maxBooks(const GfxRenderer& renderer);

  // Reads saved progress of each book; touches each book's metadata cache, so
  // run it off the first paint.
  void loadProgress(const std::vector<RecentBook>& books);
  void render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools, int toolCount,
              int selected);
  // Turns the recent row a page (dir +1 next, -1 back) and returns the new
  // selection, or -1 when there is no page that way.
  int turnPage(int selected, int dir);
  // Selectable index under a screen point, or -1.
  int hitTest(int x, int y) const;

 private:
  void drawCard(const GfxRenderer& renderer, const RecentBook* book, Rect rect, bool selected) const;
  void drawRecent(const GfxRenderer& renderer, const std::vector<RecentBook>& books, Rect rect, int coverHeight,
                  int selected);
  void drawActivity(const GfxRenderer& renderer, Rect rect) const;

  std::array<int8_t, MAX_BOOKS> progress = [] {
    std::array<int8_t, MAX_BOOKS> unknown{};
    unknown.fill(-1);
    return unknown;
  }();
  std::array<Rect, MAX_BOOKS + MAX_TOOLS> targets;
  int targetCount = 0;
  // Recent row as last drawn, for turnPage().
  int recentTotal = 0;
  int columns = 3;
  int page = 0;
};
