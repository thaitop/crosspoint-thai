#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "RecentBooksStore.h"
#include "components/HomeWidgets.h"

class GfxRenderer;

// Ledger theme home: the current book large with its progress, the other
// recent books as a progress ledger, and the tools in a slim bottom bar.
// Selection indices are flat: books first, then tools (HomeActivity order).
class LedgerHome {
 public:
  static constexpr int MAX_BOOKS = RecentBooksStore::MAX_RECENT_BOOKS;
  static constexpr int MAX_TOOLS = 5;
  static constexpr int FEATURED_COVER_HEIGHT = 225;
  static constexpr int ROW_COVER_HEIGHT = 60;

  using Tool = homeui::Tool;

  // Covers are drawn 1:1 from thumbs generated at exactly this height.
  static int coverHeightFor(size_t index) { return index == 0 ? FEATURED_COVER_HEIGHT : ROW_COVER_HEIGHT; }

  // Reads saved reading progress; touches each book's metadata cache, so run it off the first paint.
  void loadProgress(const std::vector<RecentBook>& books);
  void render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools, int toolCount,
              int selected);
  // Turns the ledger list a page (dir +1 forward, -1 back) and returns the new
  // selection, or -1 when the list fits on screen and a swipe should just step.
  int pageList(int selected, int dir);
  // Selectable index under a screen point, or -1.
  int hitTest(int x, int y) const;

 private:
  void drawRow(const GfxRenderer& renderer, const RecentBook& book, int progress, Rect rect, bool selected) const;

  std::array<int8_t, MAX_BOOKS> progress = [] {
    std::array<int8_t, MAX_BOOKS> unknown{};
    unknown.fill(-1);
    return unknown;
  }();
  std::array<Rect, MAX_BOOKS + MAX_TOOLS> targets;
  int targetCount = 0;
  // First book index shown in the ledger list; scrolls to keep the selected row visible.
  int firstRow = 1;
  int rowCount = 0;
  int visibleRows = 0;
};
