#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ThaiWordBreak.h"

namespace {

// Joins the pieces of `text` split at ThaiWordBreak's offsets with '|'.
std::string segmented(const std::string& text) {
  std::string out;
  size_t last = 0;
  for (const size_t offset : ThaiWordBreak::breakByteOffsets(text)) {
    out += text.substr(last, offset - last) + "|";
    last = offset;
  }
  return out + text.substr(last);
}

}  // namespace

TEST(ThaiWordBreak, SplitsDictionaryWords) {
  EXPECT_EQ(segmented("แล้วกดลบค้างไว้จนบรรทัดนั้นหายไปทั้งบรรทัด"), "แล้ว|กด|ลบ|ค้าง|ไว้|จน|บรรทัด|นั้น|หาย|ไป|ทั้ง|บรรทัด");
  EXPECT_EQ(segmented("นาฬิกาบนผนังบอกสีทุ่มครึ่ง"), "นาฬิกา|บน|ผนัง|บอก|สี|ทุ่ม|ครึ่ง");
}

TEST(ThaiWordBreak, PrefersLongerEarlierWordOnTie) {
  // แขก|รับ and แข|กรับ are both two dictionary words.
  EXPECT_EQ(segmented("แขกรับเชิญ"), "แขก|รับ|เชิญ");
}

TEST(ThaiWordBreak, KeepsUnknownNamesTogether) {
  // ซิ is a word but น alone is not: the lone cluster stays with it.
  EXPECT_EQ(segmented("เซียเยว่ซินยืนอยู่"), "เซียเยว่|ซิน|ยืน|อยู่");
}

TEST(ThaiWordBreak, NeverSplitsClusters) {
  for (const std::string text : {"ตั้งแต่ก่อนเปิดกอง", "ผ่านหน้าต่าง", "น้ำ"}) {
    for (const size_t offset : ThaiWordBreak::breakByteOffsets(text)) {
      // The character after a break is never a mark or following vowel.
      const std::string next = text.substr(offset, 3);
      for (const char* mark : {"ั", "่", "้", "า", "ำ", "ิ"}) EXPECT_NE(next, mark) << text;
    }
  }
}

TEST(ThaiWordBreak, BreaksOnlyInsideThaiRuns) {
  EXPECT_TRUE(ThaiWordBreak::breakByteOffsets("hello").empty());
  // No break next to the quotes or the Latin text.
  EXPECT_EQ(segmented("‘ตอนนั้นหลับแล้ว’"), "‘ตอน|นั้น|หลับ|แล้ว’");
  EXPECT_EQ(segmented("abcสีทุ่ม"), "abcสี|ทุ่ม");
}
