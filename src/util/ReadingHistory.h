#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

// Minutes read per day for the last DAYS days, ending at endDay(). A ring of
// one byte per day: slot = day % DAYS. Each day saturates at 255 minutes.
// Days are counted from 1970-01-01.
class ReadingHistory {
 public:
  // Kept at 91 so saved logs load unchanged; Shelf shows the last 30.
  static constexpr int DAYS = 91;

  // Records the day's total reading. Under a minute counts as no reading.
  // A later day moves the window and clears the days it skipped; days that
  // already left the window are ignored.
  void set(const int32_t day, const uint32_t seconds) {
    if (day < 0 || (end >= 0 && day <= end - DAYS)) return;
    if (day > end) {
      if (end < 0 || day - end >= DAYS) {
        minutes.fill(0);
      } else {
        for (int32_t d = end + 1; d < day; ++d) minutes[d % DAYS] = 0;
      }
      end = day;
    }
    minutes[day % DAYS] = seconds >= 60 ? static_cast<uint8_t>(std::min<uint32_t>(255, (seconds + 30) / 60)) : 0;
  }

  // Today's reading. Unlike set(), a day far before the window means the clock
  // was corrected backwards, so the history restarts instead of freezing.
  void record(const int32_t day, const uint32_t seconds) {
    if (end >= 0 && day <= end - DAYS) {
      minutes.fill(0);
      end = -1;
    }
    set(day, seconds);
  }

  int minutesOn(const int32_t day) const {
    if (end < 0 || day > end || day <= end - DAYS || day < 0) return 0;
    return minutes[day % DAYS];
  }

  int32_t endDay() const { return end; }

  // Restores minutes saved oldest first, the last one being endDay's.
  void load(const int32_t endDay, const uint8_t* oldestFirst, const int count) {
    minutes.fill(0);
    end = endDay;
    for (int i = 0; i < count; ++i) {
      const int32_t day = endDay - count + 1 + i;
      if (day >= 0 && day > endDay - DAYS) minutes[day % DAYS] = oldestFirst[i];
    }
  }

 private:
  std::array<uint8_t, DAYS> minutes{};
  int32_t end = -1;
};
