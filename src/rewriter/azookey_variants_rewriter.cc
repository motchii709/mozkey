// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "rewriter/azookey_variants_rewriter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

// Description shown for every candidate added by this rewriter.
constexpr absl::string_view kDescription = "azooKey 拡張変換";

// Superscript digits. Note that they are not a contiguous block in Unicode:
// '1', '2' and '3' live at U+00B9, U+00B2 and U+00B3, while the others are at
// U+2070 and U+2074-U+2079.
const char* const kSuperscriptDigits[10] = {
    "⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹",
};

// Subscript digits: U+2080-U+2089.
const char* const kSubscriptDigits[10] = {
    "₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉",
};

// Full-width digits: U+FF10-U+FF19.
const char* const kFullWidthDigits[10] = {
    "０", "１", "２", "３", "４", "５", "６", "７", "８", "９",
};

// Maps a full-width kana codepoint to its half-width katakana form, following
// the JIS X 0208 half-width katakana layout (U+FF61-U+FF9F). Half-width
// katakana has no precomposed voiced/semi-voiced letters, so they are
// expressed with the combining marks U+FF9E (ﾞ) and U+FF9F (ﾟ).
// Keys are hiragana codepoints written as UTF-32 character literals;
// katakana input is normalized to hiragana before the lookup (see
// ConvertToHalfWidthKana). ゐ/ゑ and ヰ/ヱ are deliberately omitted because
// they have no half-width katakana form.
const auto* kHalfWidthKatakanaTable =
    new absl::flat_hash_map<char32_t, absl::string_view>({
        // Monographs
        {U'あ', "ｱ"}, {U'い', "ｲ"}, {U'う', "ｳ"}, {U'え', "ｴ"}, {U'お', "ｵ"},
        {U'か', "ｶ"}, {U'き', "ｷ"}, {U'く', "ｸ"}, {U'け', "ｹ"}, {U'こ', "ｺ"},
        {U'さ', "ｻ"}, {U'し', "ｼ"}, {U'す', "ｽ"}, {U'せ', "ｾ"}, {U'そ', "ｿ"},
        {U'た', "ﾀ"}, {U'ち', "ﾁ"}, {U'つ', "ﾂ"}, {U'て', "ﾃ"}, {U'と', "ﾄ"},
        {U'な', "ﾅ"}, {U'に', "ﾆ"}, {U'ぬ', "ﾇ"}, {U'ね', "ﾈ"}, {U'の', "ﾉ"},
        {U'は', "ﾊ"}, {U'ひ', "ﾋ"}, {U'ふ', "ﾌ"}, {U'へ', "ﾍ"}, {U'ほ', "ﾎ"},
        {U'ま', "ﾏ"}, {U'み', "ﾐ"}, {U'む', "ﾑ"}, {U'め', "ﾒ"}, {U'も', "ﾓ"},
        {U'や', "ﾔ"}, {U'ゆ', "ﾕ"}, {U'よ', "ﾖ"},
        {U'ら', "ﾗ"}, {U'り', "ﾘ"}, {U'る', "ﾙ"}, {U'れ', "ﾚ"}, {U'ろ', "ﾛ"},
        {U'わ', "ﾜ"}, {U'を', "ｦ"}, {U'ん', "ﾝ"},

        // Voiced kana (dakuten, U+FF9E)
        {U'が', "ｶﾞ"}, {U'ぎ', "ｷﾞ"}, {U'ぐ', "ｸﾞ"}, {U'げ', "ｹﾞ"},
        {U'ご', "ｺﾞ"}, {U'ざ', "ｻﾞ"}, {U'じ', "ｼﾞ"}, {U'ず', "ｽﾞ"},
        {U'ぜ', "ｾﾞ"}, {U'ぞ', "ｿﾞ"}, {U'だ', "ﾀﾞ"}, {U'ぢ', "ﾁﾞ"},
        {U'づ', "ﾂﾞ"}, {U'で', "ﾃﾞ"}, {U'ど', "ﾄﾞ"}, {U'ば', "ﾊﾞ"},
        {U'び', "ﾋﾞ"}, {U'ぶ', "ﾌﾞ"}, {U'べ', "ﾍﾞ"}, {U'ぼ', "ﾎﾞ"},
        {U'ゔ', "ｳﾞ"},

        // Semi-voiced kana (handakuten, U+FF9F)
        {U'ぱ', "ﾊﾟ"}, {U'ぴ', "ﾋﾟ"}, {U'ぷ', "ﾌﾟ"}, {U'ぺ', "ﾍﾟ"},
        {U'ぽ', "ﾎﾟ"},

        // Small kana
        {U'ぁ', "ｧ"}, {U'ぃ', "ｨ"}, {U'ぅ', "ｩ"}, {U'ぇ', "ｪ"}, {U'ぉ', "ｫ"},
        {U'っ', "ｯ"}, {U'ゃ', "ｬ"}, {U'ゅ', "ｭ"}, {U'ょ', "ｮ"},

        // Prolonged sound mark (katakana only; it has no hiragana form)
        {U'ー', "ｰ"},
    });

// Returns true if every byte of `key` is an ASCII digit. A bare digit key
// never contains a multi-byte character, so a byte-wise check is sufficient.
bool IsAllDigits(absl::string_view key) {
  if (key.empty()) {
    return false;
  }
  for (const char c : key) {
    if (!absl::ascii_isdigit(c)) {
      return false;
    }
  }
  return true;
}

// Converts an all-digit key with `digits` (indexed by ASCII digit value).
std::string ConvertDigits(absl::string_view key, const char* const digits[10]) {
  std::string value;
  value.reserve(key.size() * 3);
  for (const char c : key) {
    absl::StrAppend(&value, digits[c - '0']);
  }
  return value;
}

// Converts an all-kana key into half-width katakana. Returns std::nullopt if
// any character has no half-width katakana form, in which case no candidate is
// emitted rather than a partial conversion.
std::optional<std::string> ConvertToHalfWidthKana(absl::string_view key) {
  std::string value;
  for (const char32_t codepoint : Util::Utf8ToUtf32(key)) {
    // Hiragana and katakana are parallel blocks: a katakana codepoint is the
    // matching hiragana one plus (U'ア' - U'あ'). Normalize to hiragana so a
    // single table covers both scripts.
    char32_t hiragana = codepoint;
    if (codepoint >= U'ァ' && codepoint <= U'ヶ') {
      hiragana = codepoint - (U'ア' - U'あ');
    }
    const auto it = kHalfWidthKatakanaTable->find(hiragana);
    if (it == kHalfWidthKatakanaTable->end()) {
      return std::nullopt;
    }
    absl::StrAppend(&value, it->second);
  }
  return value;
}

// Appends `value` as an extra candidate at the end of `segment`. Existing
// candidates are left untouched.
void AddCandidate(const absl::string_view key, const absl::string_view description,
                  std::string value, Segment* segment) {
  DCHECK(segment);

  converter::Candidate* candidate =
      segment->insert_candidate(segment->candidates_size());
  DCHECK(candidate);

  candidate->key = key;
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = std::move(value);
  candidate->converted_segment_count = 1;
  candidate->description = std::string(description);
  candidate->attributes |= (converter::Attribute::NO_LEARNING |
                            converter::Attribute::NO_VARIANTS_EXPANSION);
}
}  // namespace

int AzookeyVariantsRewriter::capability(
    const ConversionRequest& request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool AzookeyVariantsRewriter::Rewrite(const ConversionRequest& request,
                                      Segments* segments) const {
  if (!request.config().use_azookey_variants_conversion()) {
    return false;
  }

  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  const absl::string_view key = request.key();
  if (key.empty()) {
    return false;
  }

  // Leave SmallLetterRewriter's markup (e.g. "x^2", "CH_3") to that rewriter.
  // Both markers are ASCII and never appear inside a multi-byte sequence.
  if (key.find('^') != absl::string_view::npos ||
      key.find('_') != absl::string_view::npos) {
    return false;
  }

  Segment* segment = segments->mutable_conversion_segment(0);

  if (IsAllDigits(key)) {
    AddCandidate(key, kDescription, ConvertDigits(key, kSuperscriptDigits),
                 segment);
    AddCandidate(key, kDescription, ConvertDigits(key, kSubscriptDigits),
                 segment);
    AddCandidate(key, kDescription, ConvertDigits(key, kFullWidthDigits),
                 segment);
    return true;
  }

  if (std::optional<std::string> half_width_kana =
          ConvertToHalfWidthKana(key)) {
    AddCandidate(key, kDescription, std::move(*half_width_kana), segment);
    return true;
  }

  return false;
}

}  // namespace mozc
