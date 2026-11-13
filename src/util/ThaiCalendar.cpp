#include "ThaiCalendar.h"

#include <cmath>
#include <cstdlib>

#include "ThaiLunarTable.h"

namespace thai_cal {

namespace {

constexpr int YEAR_COUNT = sizeof(thai_lunar::YEARS) / sizeof(thai_lunar::YEARS[0]);
constexpr int MAX_MONTHS = 13;

int yearLength(const thai_lunar::YearType type) {
  return type == thai_lunar::EXTRA_MONTH ? 384 : type == thai_lunar::EXTRA_DAY ? 355 : 354;
}

// Fills month lengths in calendar order; returns the month count (12 or 13).
int monthLengths(const thai_lunar::YearType type, uint8_t* lengths) {
  int count = 0;
  for (int month = 1; month <= 12; ++month) {
    lengths[count++] = (month % 2 == 0 || (month == 7 && type == thai_lunar::EXTRA_DAY)) ? 30 : 29;
    if (month == 8 && type == thai_lunar::EXTRA_MONTH) lengths[count++] = 30;
  }
  return count;
}

int findYear(const int32_t day) {
  for (int i = 0; i < YEAR_COUNT; ++i) {
    const auto& y = thai_lunar::YEARS[i];
    if (day >= y.month1Start && day < y.month1Start + yearLength(y.type)) return i;
  }
  return -1;
}

// Day of ขึ้น/แรม `night` in lunar `month` (secondEighth picks month 8 หลัง) of table year `index`.
int32_t lunarDay(const int index, const int month, const bool secondEighth, const bool waxing, const int night) {
  const auto& y = thai_lunar::YEARS[index];
  const bool extra = y.type == thai_lunar::EXTRA_MONTH;
  uint8_t lengths[MAX_MONTHS];
  monthLengths(y.type, lengths);
  const int slot = month - 1 + ((extra && (month > 8 || secondEighth)) ? 1 : 0);
  int32_t start = y.month1Start;
  for (int i = 0; i < slot; ++i) start += lengths[i];
  return start + (waxing ? night - 1 : 15 + night - 1);
}

struct LunarHoliday {
  Holiday holiday;
  uint8_t month;       // in a normal year
  uint8_t extraMonth;  // in an extra-month year
  bool extraSecond;    // extra-month year: month 8 หลัง
  bool waxing;
  uint8_t night;
};

constexpr LunarHoliday LUNAR_HOLIDAYS[] = {
    {Holiday::MAKHA_BUCHA, 3, 4, false, true, 15}, {Holiday::VISAKHA_BUCHA, 6, 7, false, true, 15},
    {Holiday::ASALHA_BUCHA, 8, 8, true, true, 15}, {Holiday::KHAO_PHANSA, 8, 8, true, false, 1},
    {Holiday::OK_PHANSA, 11, 11, false, true, 15}, {Holiday::LOY_KRATHONG, 12, 12, false, true, 15},
};

struct FixedHoliday {
  Holiday holiday;
  uint8_t month;
  uint8_t day;  // 0 = second Saturday
};

constexpr FixedHoliday FIXED_HOLIDAYS[] = {
    {Holiday::NEW_YEAR, 1, 1},     {Holiday::CHILDRENS_DAY, 1, 0},      {Holiday::CHAKRI_DAY, 4, 6},
    {Holiday::SONGKRAN, 4, 13},    {Holiday::LABOUR_DAY, 5, 1},         {Holiday::MOTHERS_DAY, 8, 12},
    {Holiday::FATHERS_DAY, 12, 5}, {Holiday::CONSTITUTION_DAY, 12, 10}, {Holiday::NEW_YEARS_EVE, 12, 31},
};

void consider(const int32_t day, const Holiday holiday, const int32_t today, ImportantDay& best, bool& found) {
  if (day < today || (found && day >= best.day)) return;
  best = {day, holiday};
  found = true;
}

}  // namespace

int32_t daysFromCivil(int year, const int month, const int day) {
  // Howard Hinnant's days_from_civil.
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int yoe = year - era * 400;
  const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

void civilFromDays(int32_t day, int& year, int& month, int& dayOfMonth) {
  day += 719468;
  const int era = (day >= 0 ? day : day - 146096) / 146097;
  const int doe = day - era * 146097;
  const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int mp = (5 * doy + 2) / 153;
  dayOfMonth = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = yoe + era * 400 + (month <= 2);
}

bool lunarDate(const int32_t day, LunarDate& out) {
  const int index = findYear(day);
  if (index < 0) return false;
  const auto& y = thai_lunar::YEARS[index];
  uint8_t lengths[MAX_MONTHS];
  const int count = monthLengths(y.type, lengths);
  int32_t offset = day - y.month1Start;
  int slot = 0;
  while (slot < count - 1 && offset >= lengths[slot]) offset -= lengths[slot++];
  const bool extra = y.type == thai_lunar::EXTRA_MONTH;
  out.secondEighth = extra && slot == 8;
  out.month = static_cast<uint8_t>(slot + 1 - ((extra && slot >= 8) ? 1 : 0));
  out.dayInMonth = static_cast<uint8_t>(offset);
  out.waxing = offset < 15;
  out.night = static_cast<uint8_t>(out.waxing ? offset + 1 : offset - 14);
  return true;
}

uint16_t moonLitPermille(const LunarDate& date) {
  constexpr double SYNODIC_MONTH = 29.53;
  const double phase = date.dayInMonth / SYNODIC_MONTH;
  return static_cast<uint16_t>(std::lround(1000.0 * (1.0 - std::cos(2.0 * M_PI * phase)) / 2.0));
}

bool moonRowSpan(const int radius, const int dy, const uint16_t litPermille, const bool waxing, int& x0, int& x1) {
  if (dy < -radius || dy > radius) return false;
  const int halfWidth = static_cast<int>(std::lround(std::sqrt(static_cast<double>(radius * radius - dy * dy))));
  // The terminator is a half-ellipse; its x-radius shrinks to 0 at half lit.
  const int terminator = std::abs(1000 - 2 * static_cast<int>(litPermille)) * halfWidth / 1000;
  const int inner = litPermille < 500 ? terminator : -terminator;
  if (waxing) {
    x0 = inner;
    x1 = halfWidth;
  } else {
    x0 = -halfWidth;
    x1 = -inner;
  }
  return x0 < x1;
}

bool nextImportantDay(const int32_t today, ImportantDay& out) {
  bool found = false;
  int year, month, day;
  civilFromDays(today, year, month, day);
  for (int y = year; y <= year + 1; ++y) {
    for (const auto& h : FIXED_HOLIDAYS) {
      int32_t date;
      if (h.day == 0) {
        const int32_t first = daysFromCivil(y, h.month, 1);
        date = first + (6 - weekday(first)) + 7;  // second Saturday
      } else {
        date = daysFromCivil(y, h.month, h.day);
      }
      consider(date, h.holiday, today, out, found);
    }
  }
  const int index = findYear(today);
  if (index >= 0) {
    for (int i = index; i < YEAR_COUNT && i <= index + 1; ++i) {
      const bool extra = thai_lunar::YEARS[i].type == thai_lunar::EXTRA_MONTH;
      for (const auto& h : LUNAR_HOLIDAYS) {
        const int32_t date = lunarDay(i, extra ? h.extraMonth : h.month, extra && h.extraSecond, h.waxing, h.night);
        consider(date, h.holiday, today, out, found);
      }
    }
  }
  return found;
}

int zodiacIndex(const int gregorianYear) { return ((gregorianYear - 2020) % 12 + 12) % 12; }

}  // namespace thai_cal
