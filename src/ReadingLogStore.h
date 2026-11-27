#pragma once
#include <ArduinoJson.h>
#include <PersistableStore.h>

#include <array>
#include <cstdint>
#include <ctime>

#include "util/ReadingHistory.h"

// Reading time per local calendar day for the last week and for each day of
// the current month, minutes per day for the last 13 weeks, and the current
// streak of days with reading. Days are counted only while the clock is set.
class ReadingLogStore : public PersistableStore<ReadingLogStore> {
 public:
  static constexpr int DAYS = 7;

 private:
  struct Day {
    int32_t day = -1;  // days since 1970-01-01, -1 when unused
    uint32_t seconds = 0;
  };
  std::array<Day, DAYS> days{};  // newest first
  int32_t streakEnd = -1;
  uint16_t streakDays = 0;
  int32_t monthKey = -1;                    // year * 12 + tm_mon of monthSeconds
  std::array<uint32_t, 31> monthSeconds{};  // indexed by tm_mday - 1
  ReadingHistory history;

  ReadingLogStore() = default;
  ~ReadingLogStore() = default;

  friend class PersistableStore<ReadingLogStore>;

 public:
  static const char* getFilePath() { return "/.crosspoint/reading_log.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  // Local date from the RTC; false when there is no clock or it was never set.
  static bool localDate(struct tm& out);
  static int32_t dayNumber(const struct tm& date);
  // 0 = Sunday; 1970-01-01 was a Thursday.
  static int weekday(int32_t day) { return static_cast<int>((day + 4) % 7); }

  // Credits reading time to today and saves; ignored without a set clock.
  void addReading(uint32_t seconds);
  uint32_t secondsOn(int32_t day) const;
  // Seconds read on date's day, from the month log or the week log.
  uint32_t secondsOnDate(const struct tm& date) const;
  // Days with less reading than this do not count as reading days.
  static constexpr uint32_t READING_DAY_SECONDS = 60;
  // Minutes read on a day of the last ReadingHistory::DAYS days, else 0.
  int historyMinutes(int32_t day) const { return history.minutesOn(day); }
  // Consecutive reading days ending today or yesterday.
  int streak(int32_t today) const { return streakEnd >= today - 1 ? streakDays : 0; }
};

#define READING_LOG ReadingLogStore::getInstance()
