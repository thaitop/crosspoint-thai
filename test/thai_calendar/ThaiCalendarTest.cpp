#include <gtest/gtest.h>

#include <string>

#include "util/ThaiCalendar.h"
#include "util/ThaiLunarTable.h"

using namespace thai_cal;

namespace {

int32_t d(int y, int m, int day) { return daysFromCivil(y, m, day); }

std::string lunar(int32_t day) {
  LunarDate ld{};
  if (!lunarDate(day, ld)) return "none";
  return std::string(ld.waxing ? "ขึ้น " : "แรม ") + std::to_string(ld.night) + " ค่ำ เดือน " + std::to_string(ld.month) +
         (ld.secondEighth ? " หลัง" : "");
}

ImportantDay next(int32_t day) {
  ImportantDay out{};
  EXPECT_TRUE(nextImportantDay(day, out));
  return out;
}

}  // namespace

TEST(Civil, RoundTrip) {
  EXPECT_EQ(d(1970, 1, 1), 0);
  int y, m, dd;
  civilFromDays(d(2026, 11, 13), y, m, dd);
  EXPECT_EQ(y, 2026);
  EXPECT_EQ(m, 11);
  EXPECT_EQ(dd, 13);
  EXPECT_EQ(weekday(d(2026, 11, 13)), 5);  // Friday
}

TEST(LunarDate, TableEdges) {
  const auto& first = thai_lunar::YEARS[0];
  const auto& last = thai_lunar::YEARS[sizeof(thai_lunar::YEARS) / sizeof(thai_lunar::YEARS[0]) - 1];
  const int32_t lastDay = last.month1Start + 353;  // last is EXTRA_MONTH (384) or shorter; 353 is always inside
  LunarDate ld{};
  EXPECT_TRUE(lunarDate(first.month1Start, ld));
  EXPECT_FALSE(lunarDate(first.month1Start - 1, ld));
  EXPECT_TRUE(lunarDate(lastDay, ld));
  EXPECT_FALSE(lunarDate(last.month1Start + 384, ld));
}

TEST(LunarDate, MonthOneStart) {
  EXPECT_EQ(lunar(d(2025, 11, 21)), "ขึ้น 1 ค่ำ เดือน 1");
  EXPECT_EQ(lunar(d(2026, 12, 10)), "ขึ้น 1 ค่ำ เดือน 1");
  EXPECT_EQ(lunar(d(2026, 12, 9)), "แรม 15 ค่ำ เดือน 12");  // month 12 has 30 days
}

TEST(LunarDate, SecondEighth) {
  // 2026 is an extra-month year: Asalha Bucha (29 Jul) is in month 8 หลัง, Khao Phansa the day after.
  EXPECT_EQ(lunar(d(2026, 7, 29)), "ขึ้น 15 ค่ำ เดือน 8 หลัง");
  EXPECT_EQ(lunar(d(2026, 7, 30)), "แรม 1 ค่ำ เดือน 8 หลัง");
  EXPECT_EQ(lunar(d(2026, 6, 29)), "ขึ้น 15 ค่ำ เดือน 8");
  EXPECT_EQ(lunar(d(2026, 7, 15)), "ขึ้น 1 ค่ำ เดือน 8 หลัง");
}

TEST(LunarDate, OfficialDates) {
  EXPECT_EQ(lunar(d(2026, 3, 3)), "ขึ้น 15 ค่ำ เดือน 4");  // Makha Bucha in an extra-month year
  EXPECT_EQ(lunar(d(2026, 5, 31)), "ขึ้น 15 ค่ำ เดือน 7");
  EXPECT_EQ(lunar(d(2025, 11, 5)), "ขึ้น 15 ค่ำ เดือน 12");
  EXPECT_EQ(lunar(d(2026, 11, 24)), "ขึ้น 15 ค่ำ เดือน 12");
  EXPECT_EQ(lunar(d(2026, 11, 13)), "ขึ้น 4 ค่ำ เดือน 12");
}

TEST(LunarDate, ExtraDayMonthSeven) {
  // 2025 is an extra-day year: month 7 has 30 days, so it has แรม 15.
  LunarDate ld{};
  ASSERT_TRUE(lunarDate(d(2025, 5, 11), ld));            // Visakha, ขึ้น 15 ค่ำ เดือน 6
  ASSERT_TRUE(lunarDate(d(2025, 5, 11) + 16 + 29, ld));  // last day of month 7
  EXPECT_EQ(ld.month, 7);
  EXPECT_FALSE(ld.waxing);
  EXPECT_EQ(ld.night, 15);
}

TEST(Moon, NewAndFull) {
  LunarDate ld{};
  ASSERT_TRUE(lunarDate(d(2026, 11, 10), ld));
  EXPECT_LE(moonLitPermille(ld), 10);
  ASSERT_TRUE(lunarDate(d(2026, 11, 24), ld));
  EXPECT_GE(moonLitPermille(ld), 990);
  ASSERT_TRUE(lunarDate(d(2026, 11, 13), ld));
  EXPECT_TRUE(ld.waxing);
  EXPECT_GT(moonLitPermille(ld), 50);
  EXPECT_LT(moonLitPermille(ld), 200);
}

TEST(Moon, RowSpan) {
  int x0, x1;
  EXPECT_FALSE(moonRowSpan(20, 0, 0, true, x0, x1));
  ASSERT_TRUE(moonRowSpan(20, 0, 1000, true, x0, x1));
  EXPECT_EQ(x0, -20);
  EXPECT_EQ(x1, 20);
  ASSERT_TRUE(moonRowSpan(20, 0, 500, true, x0, x1));
  EXPECT_EQ(x0, 0);
  EXPECT_EQ(x1, 20);
  ASSERT_TRUE(moonRowSpan(20, 0, 500, false, x0, x1));
  EXPECT_EQ(x0, -20);
  EXPECT_EQ(x1, 0);
  ASSERT_TRUE(moonRowSpan(20, 0, 100, true, x0, x1));  // thin crescent on the right
  EXPECT_GT(x0, 10);
  EXPECT_EQ(x1, 20);
  EXPECT_FALSE(moonRowSpan(20, 21, 1000, true, x0, x1));
}

TEST(Important, SameDay) {
  const auto day = next(d(2026, 11, 24));
  EXPECT_EQ(day.day, d(2026, 11, 24));
  EXPECT_EQ(day.holiday, Holiday::LOY_KRATHONG);
}

TEST(Important, FromMockupDate) {
  const auto day = next(d(2026, 11, 13));
  EXPECT_EQ(day.day, d(2026, 11, 24));
  EXPECT_EQ(day.holiday, Holiday::LOY_KRATHONG);
}

TEST(Important, CrossesYear) {
  EXPECT_EQ(next(d(2026, 12, 31)).holiday, Holiday::NEW_YEARS_EVE);
  EXPECT_EQ(next(d(2027, 1, 1)).holiday, Holiday::NEW_YEAR);
  EXPECT_EQ(next(d(2026, 12, 11)).holiday, Holiday::NEW_YEARS_EVE);
  EXPECT_EQ(next(d(2026, 12, 6)).day, d(2026, 12, 10));
}

TEST(Important, LunarInExtraMonthYear) {
  EXPECT_EQ(next(d(2026, 7, 20)).day, d(2026, 7, 29));
  EXPECT_EQ(next(d(2026, 7, 30)).holiday, Holiday::KHAO_PHANSA);
  EXPECT_EQ(next(d(2026, 2, 20)).holiday, Holiday::MAKHA_BUCHA);
  EXPECT_EQ(next(d(2026, 2, 20)).day, d(2026, 3, 3));
}

TEST(Important, ChildrensDay) {
  EXPECT_EQ(next(d(2027, 1, 2)).holiday, Holiday::CHILDRENS_DAY);
  EXPECT_EQ(next(d(2027, 1, 2)).day, d(2027, 1, 9));
}

TEST(Important, OutsideTableFixedOnly) {
  const auto day = next(d(2060, 11, 1));
  EXPECT_EQ(day.day, d(2060, 12, 5));
  EXPECT_EQ(day.holiday, Holiday::FATHERS_DAY);
}

TEST(Zodiac, Index) {
  EXPECT_EQ(zodiacIndex(2026), 6);   // Horse, มะเมีย
  EXPECT_EQ(zodiacIndex(2020), 0);   // Rat, ชวด
  EXPECT_EQ(zodiacIndex(2031), 11);  // Pig, กุน
  EXPECT_EQ(zodiacIndex(2019), 11);
}

TEST(Week, CrossesMonth) {
  const int32_t day = d(2026, 1, 1);  // Thursday
  EXPECT_EQ(weekday(day), 4);
  int y, m, dd;
  civilFromDays(day - weekday(day), y, m, dd);
  EXPECT_EQ(y, 2025);
  EXPECT_EQ(m, 12);
  EXPECT_EQ(dd, 28);
}
