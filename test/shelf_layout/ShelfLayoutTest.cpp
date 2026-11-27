#include <gtest/gtest.h>

#include "components/ShelfLayout.h"

TEST(ShelfLayout, ThreeColumnsWhenWide) {
  EXPECT_EQ(shelf::recentColumns(362), 3);
  EXPECT_EQ(shelf::recentColumns(440), 3);
}

TEST(ShelfLayout, TwoColumnsWhenNarrow) {
  EXPECT_EQ(shelf::recentColumns(361), 2);
  EXPECT_EQ(shelf::recentColumns(250), 2);
}

TEST(ShelfLayout, RecentCountIsCappedAtNine) {
  EXPECT_EQ(shelf::recentCount(0), 0);
  EXPECT_EQ(shelf::recentCount(1), 0);
  EXPECT_EQ(shelf::recentCount(4), 3);
  EXPECT_EQ(shelf::recentCount(10), 9);
  EXPECT_EQ(shelf::recentCount(25), 9);
}

TEST(ShelfLayout, PagesRoundUp) {
  EXPECT_EQ(shelf::recentPages(0, 3), 0);
  EXPECT_EQ(shelf::recentPages(1, 3), 1);
  EXPECT_EQ(shelf::recentPages(3, 3), 1);
  EXPECT_EQ(shelf::recentPages(4, 3), 2);
  EXPECT_EQ(shelf::recentPages(9, 3), 3);
  EXPECT_EQ(shelf::recentPages(9, 2), 5);
}

TEST(ShelfLayout, HeatLevelsMatchCalendarTheme) {
  EXPECT_EQ(shelf::heatLevel(0), 0);
  EXPECT_EQ(shelf::heatLevel(1), 1);
  EXPECT_EQ(shelf::heatLevel(59), 1);
  EXPECT_EQ(shelf::heatLevel(60), 2);
  EXPECT_EQ(shelf::heatLevel(119), 2);
  EXPECT_EQ(shelf::heatLevel(120), 3);
  EXPECT_EQ(shelf::heatLevel(255), 3);
}

TEST(ShelfLayout, HeatGridSize) {
  EXPECT_EQ(shelf::HEAT_ROWS, 2);
  EXPECT_EQ(shelf::heatCell(14 * 5 + 15 * 20), 20);
  EXPECT_EQ(shelf::heatCell(2000), shelf::HEAT_MAX_CELL);
  EXPECT_EQ(shelf::heatGridWidth(20), 15 * 20 + 14 * 5);
  EXPECT_EQ(shelf::heatGridHeight(20), 2 * 20 + 5);
}

TEST(ShelfLayout, ReadingLineNeedsDateAndActivity) {
  EXPECT_FALSE(shelf::showReadingLine(false, 25, 4));
  EXPECT_FALSE(shelf::showReadingLine(true, 0, 0));
  EXPECT_TRUE(shelf::showReadingLine(true, 0, 3));
  EXPECT_TRUE(shelf::showReadingLine(true, 12, 0));
}
