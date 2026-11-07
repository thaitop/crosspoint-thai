#include "ReadingLogStore.h"

#include <HalClock.h>
#include <Logging.h>

#include <algorithm>

namespace {
// An unset RTC starts at its epoch; anything before this is not a real date.
constexpr int MIN_VALID_YEAR = 2024;

int32_t monthKeyOf(const struct tm& date) { return (date.tm_year + 1900) * 12 + date.tm_mon; }
}  // namespace

void ReadingLogStore::toJson(JsonDocument& doc) const {
  JsonArray arr = doc["days"].to<JsonArray>();
  for (const auto& entry : days) {
    if (entry.day < 0) break;
    JsonObject obj = arr.add<JsonObject>();
    obj["day"] = entry.day;
    obj["seconds"] = entry.seconds;
  }
  doc["streakEnd"] = streakEnd;
  doc["streakDays"] = streakDays;
  doc["monthKey"] = monthKey;
  JsonArray month = doc["monthSeconds"].to<JsonArray>();
  for (const uint32_t seconds : monthSeconds) month.add(seconds);
}

bool ReadingLogStore::fromJson(JsonVariantConst doc) {
  days.fill(Day{});
  int count = 0;
  for (JsonObjectConst obj : doc["days"].as<JsonArrayConst>()) {
    if (count >= DAYS) break;
    days[count++] = Day{obj["day"] | -1, obj["seconds"] | 0u};
  }
  streakEnd = doc["streakEnd"] | -1;
  streakDays = doc["streakDays"] | 0;
  monthKey = doc["monthKey"] | -1;
  monthSeconds.fill(0);
  int index = 0;
  for (JsonVariantConst seconds : doc["monthSeconds"].as<JsonArrayConst>()) {
    if (index >= static_cast<int>(monthSeconds.size())) break;
    monthSeconds[index++] = seconds | 0u;
  }
  // Earlier builds kept only a bitmask of reading days; each becomes a minimal reading day.
  if (doc["monthSeconds"].isNull()) {
    const uint32_t mask = doc["monthMask"] | 0u;
    for (int day = 0; day < static_cast<int>(monthSeconds.size()); ++day) {
      if (mask & (1u << day)) monthSeconds[day] = READING_DAY_SECONDS;
    }
  }
  LOG_DBG("RLS", "Reading log loaded (%d days)", count);
  return true;
}

bool ReadingLogStore::localDate(struct tm& out) {
  return halClock.localTime(out) && out.tm_year + 1900 >= MIN_VALID_YEAR;
}

int32_t ReadingLogStore::dayNumber(const struct tm& date) {
  // Days-from-civil (Howard Hinnant), for years after 0.
  int y = date.tm_year + 1900;
  const int m = date.tm_mon + 1;
  y -= m <= 2;
  const int era = y / 400;
  const int yoe = y - era * 400;
  const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + date.tm_mday - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void ReadingLogStore::addReading(const uint32_t seconds) {
  struct tm now;
  if (seconds == 0 || !localDate(now)) return;
  const int32_t today = dayNumber(now);

  if (days[0].day != today) {
    std::rotate(days.rbegin(), days.rbegin() + 1, days.rend());
    days[0] = Day{today, 0};
  }
  const uint32_t before = days[0].seconds;
  days[0].seconds += seconds;
  if (before < READING_DAY_SECONDS && days[0].seconds >= READING_DAY_SECONDS && streakEnd != today) {
    streakDays = streakEnd == today - 1 ? streakDays + 1 : 1;
    streakEnd = today;
  }
  const int32_t key = monthKeyOf(now);
  if (monthKey != key) {
    monthKey = key;
    monthSeconds.fill(0);
  }
  monthSeconds[now.tm_mday - 1] += seconds;
  if (!saveToFile()) LOG_ERR("RLS", "Failed to save reading log");
}

uint32_t ReadingLogStore::secondsOnDate(const struct tm& date) const {
  // The week log also covers days read before the month log existed.
  const uint32_t week = secondsOn(dayNumber(date));
  const uint32_t month = monthKey == monthKeyOf(date) ? monthSeconds[date.tm_mday - 1] : 0;
  return std::max(week, month);
}

uint32_t ReadingLogStore::secondsOn(const int32_t day) const {
  const auto it = std::find_if(days.begin(), days.end(), [day](const Day& entry) { return entry.day == day; });
  return it != days.end() ? it->seconds : 0;
}
