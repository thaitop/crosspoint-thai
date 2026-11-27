#include <gtest/gtest.h>

#include "util/ReadingHistory.h"

namespace {
constexpr int32_t TODAY = 20000;
}

TEST(ReadingHistory, EmptyReadsZero) {
  ReadingHistory h;
  EXPECT_EQ(h.endDay(), -1);
  EXPECT_EQ(h.minutesOn(TODAY), 0);
}

TEST(ReadingHistory, StoresRoundedMinutes) {
  ReadingHistory h;
  h.set(TODAY, 25 * 60 + 40);
  EXPECT_EQ(h.endDay(), TODAY);
  EXPECT_EQ(h.minutesOn(TODAY), 26);
  EXPECT_EQ(h.minutesOn(TODAY - 1), 0);
  EXPECT_EQ(h.minutesOn(TODAY + 1), 0);
}

TEST(ReadingHistory, UnderOneMinuteIsNotReading) {
  ReadingHistory h;
  h.set(TODAY, 59);
  EXPECT_EQ(h.minutesOn(TODAY), 0);
  h.set(TODAY, 60);
  EXPECT_EQ(h.minutesOn(TODAY), 1);
}

TEST(ReadingHistory, CapsAt255) {
  ReadingHistory h;
  h.set(TODAY, 10 * 3600);
  EXPECT_EQ(h.minutesOn(TODAY), 255);
}

TEST(ReadingHistory, AdvancingClearsSkippedDays) {
  ReadingHistory h;
  h.set(TODAY, 600);
  h.set(TODAY - ReadingHistory::DAYS + 3, 600);  // in window, stored
  h.set(TODAY + 5, 1200);
  EXPECT_EQ(h.minutesOn(TODAY), 10);
  EXPECT_EQ(h.minutesOn(TODAY + 5), 20);
  for (int d = 1; d < 5; ++d) EXPECT_EQ(h.minutesOn(TODAY + d), 0);
  // Slot reused from a day that left the window must read as empty.
  EXPECT_EQ(h.minutesOn(TODAY - ReadingHistory::DAYS + 3), 0);
}

TEST(ReadingHistory, LongGapClearsAll) {
  ReadingHistory h;
  h.set(TODAY, 600);
  h.set(TODAY + ReadingHistory::DAYS + 10, 120);
  EXPECT_EQ(h.minutesOn(TODAY), 0);
  for (int d = 0; d < ReadingHistory::DAYS; ++d) {
    if (d == ReadingHistory::DAYS - 1) continue;
    EXPECT_EQ(h.minutesOn(TODAY + 11 + d), 0);
  }
  EXPECT_EQ(h.minutesOn(TODAY + ReadingHistory::DAYS + 10), 2);
}

TEST(ReadingHistory, IgnoresDaysOlderThanWindow) {
  ReadingHistory h;
  h.set(TODAY, 600);
  h.set(TODAY - ReadingHistory::DAYS, 600);
  EXPECT_EQ(h.minutesOn(TODAY - ReadingHistory::DAYS), 0);
  EXPECT_EQ(h.minutesOn(TODAY), 10);
}

TEST(ReadingHistory, LoadRoundTripOldestFirst) {
  ReadingHistory a;
  a.set(TODAY - 2, 300);
  a.set(TODAY, 7200);
  // Saved oldest to newest, ending at endDay(), so the layout does not depend on the ring.
  uint8_t saved[ReadingHistory::DAYS];
  for (int i = 0; i < ReadingHistory::DAYS; ++i) saved[i] = a.minutesOn(a.endDay() - (ReadingHistory::DAYS - 1) + i);
  EXPECT_EQ(saved[ReadingHistory::DAYS - 1], 120);
  EXPECT_EQ(saved[ReadingHistory::DAYS - 3], 5);
  ReadingHistory b;
  b.load(a.endDay(), saved, ReadingHistory::DAYS);
  EXPECT_EQ(b.endDay(), TODAY);
  EXPECT_EQ(b.minutesOn(TODAY), 120);
  EXPECT_EQ(b.minutesOn(TODAY - 2), 5);
  EXPECT_EQ(b.minutesOn(TODAY - 1), 0);
}

TEST(ReadingHistory, ShortLoadEndsAtEndDay) {
  const uint8_t saved[] = {7, 9};
  ReadingHistory h;
  h.load(TODAY, saved, 2);
  EXPECT_EQ(h.minutesOn(TODAY), 9);
  EXPECT_EQ(h.minutesOn(TODAY - 1), 7);
  EXPECT_EQ(h.minutesOn(TODAY - 2), 0);
}

TEST(ReadingHistory, RecordRestartsAfterClockJumpsBack) {
  ReadingHistory h;
  h.set(TODAY + 400, 600);  // clock was set a year ahead
  h.record(TODAY, 1200);    // corrected clock
  EXPECT_EQ(h.endDay(), TODAY);
  EXPECT_EQ(h.minutesOn(TODAY), 20);
  EXPECT_EQ(h.minutesOn(TODAY + 400), 0);
}

TEST(ReadingHistory, RecordKeepsWindowForRecentDays) {
  ReadingHistory h;
  h.set(TODAY, 600);
  h.record(TODAY - 3, 1200);  // within the window: stored, end unchanged
  EXPECT_EQ(h.endDay(), TODAY);
  EXPECT_EQ(h.minutesOn(TODAY), 10);
  EXPECT_EQ(h.minutesOn(TODAY - 3), 20);
}
