#pragma once

#include <cstdint>

// Thai calendar rules for the Thai calendar sleep screen. Days are counted from
// 1970-01-01 (same as ReadingLogStore::dayNumber). Text is left to the caller.
namespace thai_cal {

int32_t daysFromCivil(int year, int month, int day);  // month 1-12
void civilFromDays(int32_t day, int& year, int& month, int& dayOfMonth);
inline int weekday(int32_t day) { return static_cast<int>(((day + 4) % 7 + 7) % 7); }  // 0 = Sunday

struct LunarDate {
  uint8_t month;       // 1-12
  bool secondEighth;   // เดือน 8 หลัง in an extra-month year
  bool waxing;         // ขึ้น, else แรม
  uint8_t night;       // ค่ำ, 1-15
  uint8_t dayInMonth;  // 0-based
};

// False outside the lunar table (thai_lunar::YEARS).
bool lunarDate(int32_t day, LunarDate& out);

// Lit part of the moon in 1/1000.
uint16_t moonLitPermille(const LunarDate& date);
// Lit x-range, relative to the disc centre, on row dy of a disc of this radius.
// False when the row has no lit pixels. Waxing is lit on the right.
bool moonRowSpan(int radius, int dy, uint16_t litPermille, bool waxing, int& x0, int& x1);

// Fixed days first, then lunar days; the order is part of the i18n tables.
enum class Holiday : uint8_t {
  NEW_YEAR,
  CHILDRENS_DAY,
  CHAKRI_DAY,
  SONGKRAN,
  LABOUR_DAY,
  MOTHERS_DAY,
  FATHERS_DAY,
  CONSTITUTION_DAY,
  NEW_YEARS_EVE,
  MAKHA_BUCHA,
  VISAKHA_BUCHA,
  ASALHA_BUCHA,
  KHAO_PHANSA,
  OK_PHANSA,
  LOY_KRATHONG,
  COUNT
};

struct ImportantDay {
  int32_t day;
  Holiday holiday;
};
// Earliest important day on or after today. Lunar days only inside the table.
bool nextImportantDay(int32_t today, ImportantDay& out);

// 0 = Rat (ชวด) ... 11 = Pig (กุน).
int zodiacIndex(int gregorianYear);

}  // namespace thai_cal
