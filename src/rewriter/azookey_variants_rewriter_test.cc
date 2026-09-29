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

#include <cstddef>
#include <string>

#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void AddSegment(absl::string_view key, absl::string_view value,
                Segments* segments) {
  Segment* seg = segments->add_segment();
  converter::Candidate* candidate = seg->add_candidate();
  seg->set_key(key);
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->value, value);
  strings::Assign(candidate->content_value, value);
}

void InitSegments(absl::string_view key, absl::string_view value,
                  Segments* segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

// Appends another candidate to the first segment of `segments`.
void AddCandidateToFirstSegment(absl::string_view key, absl::string_view value,
                                Segments* segments) {
  converter::Candidate* candidate =
      segments->mutable_segment(0)->add_candidate();
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->value, value);
  strings::Assign(candidate->content_value, value);
}

bool ContainCandidate(const Segments& segments,
                      const absl::string_view candidate) {
  const Segment& segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

ConversionRequest MakeRequest(absl::string_view key,
                              const config::Config& config) {
  return ConversionRequestBuilder().SetConfig(config).SetKey(key).Build();
}

config::Config MakeConfig(bool enabled) {
  config::Config config;
  config.set_use_azookey_variants_conversion(enabled);
  return config;
}

TEST(AzookeyVariantsRewriterTest, DigitKey) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  struct TestCase {
    absl::string_view key;
    absl::string_view superscript;
    absl::string_view subscript;
    absl::string_view full_width;
  };
  constexpr TestCase kTestCases[] = {
      {"0", "⁰", "₀", "０"},
      {"1", "¹", "₁", "１"},
      {"2", "²", "₂", "２"},
      {"3", "³", "₃", "３"},
      {"4", "⁴", "₄", "４"},
      {"9", "⁹", "₉", "９"},
      {"123", "¹²³", "₁₂₃", "１２３"},
      {"09012345678", "⁰⁹⁰¹²³⁴⁵⁶⁷⁸", "₀₉₀₁₂₃₄₅₆₇₈", "０９０１２３４５６７８"},
  };
  for (const TestCase& test_case : kTestCases) {
    Segments segments;
    InitSegments(test_case.key, test_case.key, &segments);
    EXPECT_TRUE(rewriter.Rewrite(MakeRequest(test_case.key, config), &segments));
    EXPECT_TRUE(ContainCandidate(segments, test_case.superscript));
    EXPECT_TRUE(ContainCandidate(segments, test_case.subscript));
    EXPECT_TRUE(ContainCandidate(segments, test_case.full_width));
    // The original candidate is kept and the three extras are appended.
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 4);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              test_case.key);
  }
}

TEST(AzookeyVariantsRewriterTest, KanaKey) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  struct TestCase {
    absl::string_view key;
    absl::string_view half_width_kana;
  };
  constexpr TestCase kTestCases[] = {
      {"あ", "ｱ"},
      {"ん", "ﾝ"},
      {"を", "ｦ"},
      {"は", "ﾊ"},
      {"ば", "ﾊﾞ"},
      {"ぱ", "ﾊﾟ"},
      {"が", "ｶﾞ"},
      {"ぴ", "ﾋﾟ"},
      {"ゔ", "ｳﾞ"},
      {"ぁ", "ｧ"},
      {"ょ", "ｮ"},
      {"ー", "ｰ"},
      {"らーめん", "ﾗｰﾒﾝ"},
      // Katakana input is normalized to hiragana before the lookup.
      {"ア", "ｱ"},
      {"ガ", "ｶﾞ"},
      {"パ", "ﾊﾟ"},
      {"ヴ", "ｳﾞ"},
  };
  for (const TestCase& test_case : kTestCases) {
    Segments segments;
    InitSegments(test_case.key, test_case.key, &segments);
    EXPECT_TRUE(rewriter.Rewrite(MakeRequest(test_case.key, config), &segments));
    EXPECT_TRUE(ContainCandidate(segments, test_case.half_width_kana));
  }
}

TEST(AzookeyVariantsRewriterTest, KanaWithoutHalfWidthFormIsSkipped) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  // ゐ/ゑ/ヰ/ヱ have no half-width katakana form, so no candidate is emitted
  // rather than a partial conversion.
  for (const absl::string_view key : {"ゐ", "ゑ", "ヰ", "ヱ", "あゐ"}) {
    Segments segments;
    InitSegments(key, key, &segments);
    EXPECT_FALSE(rewriter.Rewrite(MakeRequest(key, config), &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }
}

TEST(AzookeyVariantsRewriterTest, DoesNotFireOnNonKanaNonDigitKey) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  for (const absl::string_view key :
       {"あ1", "1あ", "abc", "abc123", "漢字", "x2", "mozc", "1.5", "-5"}) {
    Segments segments;
    InitSegments(key, key, &segments);
    EXPECT_FALSE(rewriter.Rewrite(MakeRequest(key, config), &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }
}

TEST(AzookeyVariantsRewriterTest, DoesNotFireOnSmallLetterMarkup) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  // SmallLetterRewriter owns '^' and '_' markup; leave it alone.
  for (const absl::string_view key : {"^123", "_123", "x^2", "CH_3", "^^"}) {
    Segments segments;
    InitSegments(key, key, &segments);
    EXPECT_FALSE(rewriter.Rewrite(MakeRequest(key, config), &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }
}

TEST(AzookeyVariantsRewriterTest, DisabledByConfig) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(false);

  for (const absl::string_view key : {"123", "あ", "が", "らーめん"}) {
    Segments segments;
    InitSegments(key, key, &segments);
    EXPECT_FALSE(rewriter.Rewrite(MakeRequest(key, config), &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }
}

TEST(AzookeyVariantsRewriterTest, ExistingCandidatesArePreserved) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  Segments segments;
  InitSegments("123", "百二十三", &segments);
  AddCandidateToFirstSegment("123", "123", &segments);
  const Segment& segment = segments.conversion_segment(0);
  ASSERT_EQ(segment.candidates_size(), 2);

  EXPECT_TRUE(rewriter.Rewrite(MakeRequest("123", config), &segments));

  ASSERT_EQ(segment.candidates_size(), 5);
  // Existing candidates keep their value and position.
  EXPECT_EQ(segment.candidate(0).value, "百二十三");
  EXPECT_EQ(segment.candidate(1).value, "123");
  // The new candidates are appended at the end.
  EXPECT_EQ(segment.candidate(2).value, "¹²³");
  EXPECT_EQ(segment.candidate(3).value, "₁₂₃");
  EXPECT_EQ(segment.candidate(4).value, "１２３");

  for (size_t i = 2; i < segment.candidates_size(); ++i) {
    const converter::Candidate& candidate = segment.candidate(i);
    EXPECT_EQ(candidate.key, "123");
    EXPECT_EQ(candidate.content_key, "123");
    EXPECT_EQ(candidate.description, "azooKey 拡張変換");
    EXPECT_NE(candidate.attributes & converter::Attribute::NO_LEARNING, 0);
    EXPECT_NE(candidate.attributes & converter::Attribute::NO_VARIANTS_EXPANSION,
              0);
  }
}

TEST(AzookeyVariantsRewriterTest, DoesNotDuplicateT13nCandidates) {
  AzookeyVariantsRewriter rewriter;
  const config::Config config = MakeConfig(true);

  // mozc's t13n rewriter already emits half-width katakana for a kana key and
  // full-width digits for a digit key (as meta candidates). Duplicating them
  // would reorder the deduplicated candidate list and break golden scenario
  // tests, so they must be skipped.
  {
    Segments segments;
    InitSegments("あ", "亜", &segments);
    converter::Candidate* meta =
        segments.mutable_segment(0)->add_meta_candidate();
    strings::Assign(meta->value, "ｱ");

    EXPECT_FALSE(rewriter.Rewrite(MakeRequest("あ", config), &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
  }

  {
    Segments segments;
    InitSegments("123", "百二十三", &segments);
    converter::Candidate* meta =
        segments.mutable_segment(0)->add_meta_candidate();
    strings::Assign(meta->value, "１２３");

    EXPECT_TRUE(rewriter.Rewrite(MakeRequest("123", config), &segments));

    const Segment& segment = segments.conversion_segment(0);
    // Only the superscript and subscript variants are new.
    ASSERT_EQ(segment.candidates_size(), 3);
    EXPECT_EQ(segment.candidate(1).value, "¹²³");
    EXPECT_EQ(segment.candidate(2).value, "₁₂₃");
  }
}

}  // namespace
}  // namespace mozc
