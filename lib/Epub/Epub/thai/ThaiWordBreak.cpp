#include "ThaiWordBreak.h"

#include <Utf8.h>

#include <algorithm>
#include <cstdint>
#include <limits>

#include "generated/thai-dict.h"

namespace {

constexpr bool isThai(const uint32_t cp) { return cp >= 0x0E01 && cp <= 0x0E5B; }

// Matches the dictionary's TIS-620 encoding.
constexpr uint8_t toTis620(const uint32_t cp) { return static_cast<uint8_t>(cp - 0x0E00 + 0xA0); }

// True when `left` and `right` belong to one cluster: a leading vowel binds to
// the next letter; marks, following vowels, PAIYANNOI and MAI YAMOK bind to the
// previous one.
constexpr bool sameCluster(const uint32_t left, const uint32_t right) {
  return (left >= 0x0E40 && left <= 0x0E44) || right == 0x0E2F || right == 0x0E30 || right == 0x0E31 ||
         (right >= 0x0E32 && right <= 0x0E3A) || (right >= 0x0E45 && right <= 0x0E4E);
}

enum class Match : uint8_t { None, Prefix, Word };

// Compares NUL-terminated dictionary `word` with `key[0, len)`. Returns <0,
// 0 or >0 like strcmp; `extendsKey` is set when `word` starts with the key.
int compareWord(const uint8_t* word, const uint8_t* key, const size_t len, bool& extendsKey) {
  extendsKey = false;
  for (size_t i = 0; i < len; ++i) {
    if (word[i] == 0) return -1;
    if (word[i] != key[i]) return word[i] < key[i] ? -1 : 1;
  }
  if (word[len] == 0) return 0;
  extendsKey = true;
  return 1;
}

Match lookup(const uint8_t* key, const size_t len) {
  using namespace thai_dict;
  constexpr size_t sampleCount = sizeof(kSampleOffsets) / sizeof(kSampleOffsets[0]);
  // Last sampled word <= key, then scan its block for the first word >= key.
  size_t lo = 0;
  size_t hi = sampleCount;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    bool extendsKey;
    if (compareWord(kWords + kSampleOffsets[mid], key, len, extendsKey) <= 0) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  const uint8_t* word = kWords + kSampleOffsets[lo];
  const uint8_t* const end = kWords + sizeof(kWords);
  for (size_t i = 0; i <= kSampleStride && word < end; ++i) {
    bool extendsKey;
    const int cmp = compareWord(word, key, len, extendsKey);
    if (cmp == 0) return Match::Word;
    if (cmp > 0) return extendsKey ? Match::Prefix : Match::None;
    while (*word) ++word;
    ++word;
  }
  return Match::None;
}

struct Letter {
  uint32_t cp;
  size_t byteOffset;
};

// Appends the word boundaries inside letters[0, n) (one Thai run) to `out`.
// Picks the segmentation with the fewest unknown clusters, then the fewest
// words; ties prefer longer earlier words. Unknown clusters merge into one
// segment, and a lone unknown cluster stays attached to the word before it.
void segmentRun(const Letter* letters, const size_t n, std::vector<size_t>& out) {
  if (n < 2 || n > std::numeric_limits<uint16_t>::max()) return;

  std::vector<uint8_t> tis(n);
  std::vector<bool> boundary(n + 1, true);
  for (size_t i = 0; i < n; ++i) {
    tis[i] = toTis620(letters[i].cp);
    if (i > 0) boundary[i] = !sameCluster(letters[i - 1].cp, letters[i].cp);
  }

  // Cost: unknown clusters in the high half, words in the low half.
  constexpr uint32_t UNKNOWN = 1u << 16;
  constexpr uint32_t INF = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> cost(n + 1, INF);
  std::vector<uint16_t> prev(n + 1, 0);
  std::vector<bool> known(n + 1, false);  // whether the edge ending here is a dictionary word
  cost[0] = 0;

  for (size_t i = 0; i < n; ++i) {
    if (cost[i] == INF || !boundary[i]) continue;
    const size_t maxLen = std::min(thai_dict::kMaxWordLength, n - i);
    for (size_t len = 1; len <= maxLen; ++len) {
      const Match match = lookup(tis.data() + i, len);
      if (match == Match::None) break;
      const size_t j = i + len;
      // `<=`: a later start means a longer earlier word at equal cost.
      if (match == Match::Word && boundary[j] && cost[i] + 1 <= cost[j]) {
        cost[j] = cost[i] + 1;
        prev[j] = static_cast<uint16_t>(i);
        known[j] = true;
      }
    }
    size_t j = i + 1;
    while (j < n && !boundary[j]) ++j;
    if (cost[i] + UNKNOWN < cost[j]) {
      cost[j] = cost[i] + UNKNOWN;
      prev[j] = static_cast<uint16_t>(i);
      known[j] = false;
    }
  }

  // Walk the best path backwards into segment ends, merging unknown clusters.
  struct Segment {
    size_t start;
    bool known;
    bool loneCluster;
  };
  std::vector<Segment> segments;
  for (size_t j = n; j > 0; j = prev[j]) {
    const size_t i = prev[j];
    if (!known[j] && !segments.empty() && !segments.back().known) {
      segments.back().start = i;
      segments.back().loneCluster = false;
    } else {
      segments.push_back({i, known[j], !known[j]});
    }
  }
  // segments runs last-to-first; the first segment has no break before it.
  for (size_t k = segments.size() - 1; k-- > 0;) {
    const Segment& segment = segments[k];
    if (!segment.known && segment.loneCluster) continue;
    out.push_back(letters[segment.start].byteOffset);
  }
}

}  // namespace

namespace ThaiWordBreak {

std::vector<size_t> breakByteOffsets(const std::string& word) {
  // Thai letters are U+0E01..U+0E5B: UTF-8 lead byte 0xE0 then 0xB8 or 0xB9.
  if (word.find("\xE0\xB8") == std::string::npos && word.find("\xE0\xB9") == std::string::npos) return {};

  std::vector<Letter> letters;
  std::vector<size_t> out;
  const auto* const start = reinterpret_cast<const unsigned char*>(word.c_str());
  const auto* ptr = start;
  while (*ptr) {
    const size_t offset = static_cast<size_t>(ptr - start);
    const uint32_t cp = utf8NextCodepoint(&ptr);
    if (isThai(cp)) {
      letters.push_back({cp, offset});
      continue;
    }
    segmentRun(letters.data(), letters.size(), out);
    letters.clear();
  }
  segmentRun(letters.data(), letters.size(), out);
  return out;
}

}  // namespace ThaiWordBreak
