#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Thai is written without spaces between words. Line breaks must fall between
// words, so each run of Thai letters is segmented with an embedded dictionary.
namespace ThaiWordBreak {

// Byte offsets inside `word` where a line may break with no visible gap.
// Only boundaries with Thai letters on both sides are returned; empty when
// `word` contains no Thai.
std::vector<size_t> breakByteOffsets(const std::string& word);

}  // namespace ThaiWordBreak
