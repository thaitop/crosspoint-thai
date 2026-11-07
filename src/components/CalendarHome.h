#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <vector>

#include "RecentBooksStore.h"
#include "components/HomeWidgets.h"

class GfxRenderer;

// Calendar theme home: a dot-matrix desk clock beside today's reading time
// as a grid of ten-minute dots, the month as a grid of day discs inked by
// how long each day was read (tap a day for its minutes), the current book
// with its resume button, and the tools in a bottom bar.
// Selection indices are flat: the current book (when any), then tools.
class CalendarHome {
 public:
  static constexpr int MAX_BOOKS = 1;
  static constexpr int MAX_TOOLS = 5;
  static constexpr int COVER_HEIGHT = 150;

  using Tool = homeui::Tool;

  // Reads the current book's saved progress; touches its metadata cache, so
  // run it off the first paint.
  void loadProgress(const std::vector<RecentBook>& books);
  void render(const GfxRenderer& renderer, const std::vector<RecentBook>& books, const Tool* tools, int toolCount,
              int selected);
  // Selectable index under a screen point, or -1.
  int hitTest(int x, int y) const;
  // True once the clock has moved past the minute last drawn, or a tapped
  // day's reading time has been shown long enough.
  bool needsRedraw() const;
  // Shows the reading time of the day under a screen point; tapping it
  // again hides it. False when the point is on no past or current day.
  bool selectDayAt(int x, int y);
  // True once per hour turn since the last call: the clock's partial
  // refreshes leave ghosting that a cleaner refresh clears.
  bool takeHourTurn();

 private:
  void drawClock(const GfxRenderer& renderer, Rect rect, const struct tm* date) const;
  void drawReadToday(const GfxRenderer& renderer, Rect rect, int minutes) const;
  void drawMonth(const GfxRenderer& renderer, Rect rect, const struct tm* date);

  int8_t currentProgress = -1;
  int16_t shownMinute = -1;  // minute of day on screen, -1 without a clock
  bool hourTurned = false;
  // Day grid as last drawn, for selectDayAt(); dayCount 0 when not drawn.
  int gridX = 0;
  int gridY = 0;
  int colW = 0;
  int rowH = 0;
  int firstWeekday = 0;
  int dayCount = 0;
  int today = 0;        // tm_mday
  int selectedDay = 0;  // tm_mday with its time shown, 0 for none
  unsigned long selectedAt = 0;
  std::array<Rect, MAX_BOOKS + MAX_TOOLS> targets;
  int targetCount = 0;
};
