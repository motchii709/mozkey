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

#include "session/zenz_adoption_policy.h"

#include <string>
#include <utility>
#include <vector>

#include "testing/gunit.h"

namespace mozc::session {
namespace {

ProtectedConversionSpan BuildSpan(
    std::string key, std::string value, ProtectedConversionSpan::Tier tier,
    bool repairable) {
  ProtectedConversionSpan span;
  span.key = std::move(key);
  span.value = std::move(value);
  span.tier = tier;
  span.repairable = repairable;
  return span;
}

TEST(ZenzAdoptionPolicyTest, ClassifiesNumericLiteralSurfaces) {
  EXPECT_EQ(ClassifyProtectedAsciiSurface("1"),
            ProtectedConversionSpan::Tier::kNumericLiteral);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("100"),
            ProtectedConversionSpan::Tier::kNumericLiteral);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("-3"),
            ProtectedConversionSpan::Tier::kNumericLiteral);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("+2"),
            ProtectedConversionSpan::Tier::kNumericLiteral);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("3.14"),
            ProtectedConversionSpan::Tier::kNumericLiteral);

  EXPECT_EQ(ClassifyProtectedAsciiSurface("Windows11"),
            ProtectedConversionSpan::Tier::kIdentityCritical);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("GPT-5"),
            ProtectedConversionSpan::Tier::kIdentityCritical);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("UTF-8"),
            ProtectedConversionSpan::Tier::kIdentityCritical);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("C++17"),
            ProtectedConversionSpan::Tier::kIdentityCritical);
  EXPECT_EQ(ClassifyProtectedAsciiSurface("1.2.3"),
            ProtectedConversionSpan::Tier::kIdentityCritical);
}

TEST(ZenzAdoptionPolicyTest, DoesNotPlaceholderNumericLiteral) {
  ZenzAdoptionPolicy policy;
  ZenzProtectedPromptInput input;
  input.key = "1にちめのかれはてんてきです";
  input.protected_spans = {
      BuildSpan("1", "1", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzProtectedPromptResult prompt = policy.ProtectPromptKey(input);
  EXPECT_EQ(prompt.placeholder_count, 0);
  EXPECT_EQ(prompt.key, "1にちめのかれはてんてきです");
  ASSERT_EQ(prompt.protected_spans.size(), 1);
  EXPECT_TRUE(prompt.protected_spans[0].placeholder.empty());
}

TEST(ZenzAdoptionPolicyTest, AcceptsNumericLiteralWithJapaneseContext) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "1にちめのかれはてんてきです";
  input.mozc_value = "1日目の彼は点滴です";
  input.zenz_value = "1日目の彼は天敵です";
  input.protected_spans = {
      BuildSpan("1", "1", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "1日目の彼は天敵です");
}

TEST(ZenzAdoptionPolicyTest, RejectsNumericJapaneseNeighborToSymbol) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "100えん";
  input.mozc_value = "100円";
  input.zenz_value = "100¥";
  input.protected_spans = {
      BuildSpan("100", "100", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "100円");
  EXPECT_EQ(result.reason, "protected_numeric_literal_context_changed");
}

TEST(ZenzAdoptionPolicyTest, AcceptsNumericJapaneseNeighborRewrite) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "すまーとふぉん1だい";
  input.mozc_value = "スマートフォン1代";
  input.zenz_value = "スマートフォン1台";
  input.protected_spans = {
      BuildSpan("1", "1", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "スマートフォン1台");
}

TEST(ZenzAdoptionPolicyTest, AcceptsNumericBetweenJapaneseCharacters) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "だい1かいのしけん";
  input.mozc_value = "第1回の試験";
  input.zenz_value = "第1回のテスト";
  input.protected_spans = {
      BuildSpan("1", "1", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "第1回のテスト");
}

TEST(ZenzAdoptionPolicyTest, RejectsNumericSymbolNeighborChange) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "100%せいこう";
  input.mozc_value = "100%成功";
  input.zenz_value = "100$成功";
  input.protected_spans = {
      BuildSpan("100", "100", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.reason, "protected_numeric_literal_context_changed");
}

TEST(ZenzAdoptionPolicyTest, AcceptsDecimalWithJapaneseContext) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "3.14ばいではたりない";
  input.mozc_value = "3.14倍では足りない";
  input.zenz_value = "3.14倍では不足です";
  input.protected_spans = {
      BuildSpan("3.14", "3.14",
                ProtectedConversionSpan::Tier::kNumericLiteral, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "3.14倍では不足です");
}

TEST(ZenzAdoptionPolicyTest, RejectsNumericDelimiterChange) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "2026/09/23にじっし";
  input.mozc_value = "2026/09/23に実施";
  input.zenz_value = "2026-09-23に実施";
  input.protected_spans = {
      BuildSpan("2026", "2026",
                ProtectedConversionSpan::Tier::kNumericLiteral, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.reason, "protected_numeric_literal_context_changed");
}

TEST(ZenzAdoptionPolicyTest, PreservesRepeatedNumericContextsIndependently) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "2にちとHTTP/2";
  input.mozc_value = "2日とHTTP/2";
  input.zenz_value = "2台とHTTP/2";
  ProtectedConversionSpan span =
      BuildSpan("2", "2", ProtectedConversionSpan::Tier::kNumericLiteral,
                false);
  span.required_occurrences = 2;
  input.protected_spans = {span};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "2台とHTTP/2");
}

TEST(ZenzAdoptionPolicyTest, RejectsRepeatedNumericTechnicalDelimiterChange) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "2にちとHTTP/2";
  input.mozc_value = "2日とHTTP/2";
  input.zenz_value = "2台とHTTP 2";
  ProtectedConversionSpan span =
      BuildSpan("2", "2", ProtectedConversionSpan::Tier::kNumericLiteral,
                false);
  span.required_occurrences = 2;
  input.protected_spans = {span};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.reason, "protected_numeric_literal_context_changed");
}

TEST(ZenzAdoptionPolicyTest, RejectsChangedNumericLiteral) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "100えん";
  input.mozc_value = "100円";
  input.zenz_value = "百円";
  input.protected_spans = {
      BuildSpan("100", "100", ProtectedConversionSpan::Tier::kNumericLiteral,
                false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.reason, "protected_numeric_literal_not_preserved");
}

TEST(ZenzAdoptionPolicyTest, ProtectPromptKeyAndRestorePlaceholder) {
  ZenzAdoptionPolicy policy;
  ZenzProtectedPromptInput input;
  input.key = "てんてきのかれはもずきーをつかっています";
  input.protected_spans = {
      BuildSpan("もずきー", "Mozkey",
                ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzProtectedPromptResult prompt = policy.ProtectPromptKey(input);
  ASSERT_EQ(prompt.protected_spans.size(), 1);
  EXPECT_EQ(prompt.placeholder_count, 1);
  EXPECT_EQ(prompt.key,
            "てんてきのかれは__MOZC_ZENZ_PROTECTED_0__をつかっています");

  const std::string restored = policy.RestorePlaceholders(
      "点滴の彼は__MOZC_ZENZ_PROTECTED_0__を使用しています",
      prompt.protected_spans);
  EXPECT_EQ(restored, "点滴の彼はMozkeyを使用しています");
}

TEST(ZenzAdoptionPolicyTest, DoesNotPlaceholderUserPreferredJapaneseSurface) {
  ZenzAdoptionPolicy policy;
  ZenzProtectedPromptInput prompt_input;
  prompt_input.key = "かれはじしょごのてんてきです";
  prompt_input.protected_spans = {
      BuildSpan("じしょご", "辞書語",
                ProtectedConversionSpan::Tier::kUserPreferred, false),
  };

  const ZenzProtectedPromptResult prompt = policy.ProtectPromptKey(prompt_input);
  ASSERT_EQ(prompt.protected_spans.size(), 1);
  EXPECT_EQ(prompt.placeholder_count, 0);
  EXPECT_EQ(prompt.key, "かれはじしょごのてんてきです");
  EXPECT_TRUE(prompt.protected_spans[0].placeholder.empty());

  ZenzAdoptionInput adoption_input;
  adoption_input.key = prompt.key;
  adoption_input.mozc_value = "彼は辞書語の点滴です";
  adoption_input.zenz_value = "彼は辞書語けの天敵です";
  adoption_input.protected_spans = prompt.protected_spans;

  const ZenzAdoptionResult result = policy.Decide(adoption_input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "彼は辞書語の天敵です");
}

TEST(ZenzAdoptionPolicyTest, UserPreferredJapaneseSurfaceIsGuardedOnOutputSide) {
  ZenzAdoptionPolicy policy;

  ProtectedConversionSpan span;
  span.key = "じしょご";
  span.value = "辞書語";
  span.tier = ProtectedConversionSpan::Tier::kUserPreferred;
  span.repairable = false;
  span.required_occurrences = 1;

  ZenzProtectedPromptInput prompt_input;
  prompt_input.key = "かれはじしょごのてんてきです";
  prompt_input.protected_spans = {span};

  const ZenzProtectedPromptResult prompt =
      policy.ProtectPromptKey(prompt_input);
  EXPECT_EQ(prompt.placeholder_count, 0);
  EXPECT_EQ(prompt.key, "かれはじしょごのてんてきです");

  ZenzAdoptionInput adoption_input;
  adoption_input.key = "かれはじしょごのてんてきです";
  adoption_input.mozc_value = "彼は辞書語の点滴です";
  adoption_input.zenz_value = "彼は辞書語けの天敵です";
  adoption_input.protected_spans = {span};

  const ZenzAdoptionResult result = policy.Decide(adoption_input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "彼は辞書語の天敵です");
}

TEST(ZenzAdoptionPolicyTest, ProtectPromptKeyHandlesRepeatedSurface) {
  ZenzAdoptionPolicy policy;
  ZenzProtectedPromptInput input;
  input.key = "もずきーともずきーをくらべます";
  ProtectedConversionSpan span = BuildSpan(
      "もずきー", "Mozkey",
      ProtectedConversionSpan::Tier::kIdentityCritical, true);
  span.required_occurrences = 2;
  input.protected_spans = {span};

  const ZenzProtectedPromptResult prompt = policy.ProtectPromptKey(input);
  ASSERT_EQ(prompt.protected_spans.size(), 1);
  EXPECT_EQ(prompt.placeholder_count, 2);
  EXPECT_EQ(prompt.key,
            "__MOZC_ZENZ_PROTECTED_0__と"
            "__MOZC_ZENZ_PROTECTED_0__をくらべます");

  const std::string restored = policy.RestorePlaceholders(
      "__MOZC_ZENZ_PROTECTED_0__と"
      "__MOZC_ZENZ_PROTECTED_0__を比較します",
      prompt.protected_spans);
  EXPECT_EQ(restored, "MozkeyとMozkeyを比較します");
}

TEST(ZenzAdoptionPolicyTest, AcceptsWhenProtectedSurfaceIsPreserved) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきーをつかっています";
  input.mozc_value = "Mozkeyを使っています";
  input.zenz_value = "Mozkeyを使用しています";
  input.protected_spans = {
      BuildSpan("もずきー", "Mozkey", ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "Mozkeyを使用しています");
}

TEST(ZenzAdoptionPolicyTest, RepairsIdentityCriticalKanaSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきーをつかっています";
  input.mozc_value = "Mozkeyを使っています";
  input.zenz_value = "モズキーを使用しています";
  input.protected_spans = {
      BuildSpan("もずきー", "Mozkey", ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "Mozkeyを使用しています");
}

TEST(ZenzAdoptionPolicyTest, AcceptsJapaneseUserDictionarySurfaceWithParticle) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "かれはじしょごのてんてきです";
  input.mozc_value = "彼は辞書語の点滴です";
  input.zenz_value = "彼は辞書語の天敵です";
  input.protected_spans = {
      BuildSpan("じしょご", "辞書語",
                ProtectedConversionSpan::Tier::kUserPreferred, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "彼は辞書語の天敵です");
}

TEST(ZenzAdoptionPolicyTest, RepairsAttachedKanaAfterProtectedSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "かれはじしょごのてんてきです";
  input.mozc_value = "彼は辞書語の点滴です";
  input.zenz_value = "彼は辞書語けの天敵です";
  input.protected_spans = {
      BuildSpan("じしょご", "辞書語",
                ProtectedConversionSpan::Tier::kUserPreferred, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "彼は辞書語の天敵です");
}

TEST(ZenzAdoptionPolicyTest, RejectsAttachedKanaWithoutSafeBoundaryRepair) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "かれはじしょご";
  input.mozc_value = "彼は辞書語";
  input.zenz_value = "彼は辞書語け";
  input.protected_spans = {
      BuildSpan("じしょご", "辞書語",
                ProtectedConversionSpan::Tier::kUserPreferred, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "彼は辞書語");
}

TEST(ZenzAdoptionPolicyTest, RejectsAsciiIdentitySurfaceAttachment) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきーをつかっています";
  input.mozc_value = "Mozkeyを使っています";
  input.zenz_value = "MozkeyXを使用しています";
  input.protected_spans = {
      BuildSpan("もずきー", "Mozkey",
                ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "Mozkeyを使っています");
}

TEST(ZenzAdoptionPolicyTest, RejectsAmbiguousRepair) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきー";
  input.mozc_value = "MozkeyとMozkey";
  input.zenz_value = "モズキーとモズキー";
  input.protected_spans = {
      BuildSpan("もずきー", "Mozkey", ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.reason, "protected_user_dictionary_surface_not_preserved");
}

TEST(ZenzAdoptionPolicyTest, RepairsOneMissingRepeatedSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきーともずきー";
  input.mozc_value = "MozkeyとMozkey";
  input.zenz_value = "Mozkeyとモズキー";

  ProtectedConversionSpan span = BuildSpan(
      "もずきー", "Mozkey",
      ProtectedConversionSpan::Tier::kIdentityCritical, true);
  span.required_occurrences = 2;
  input.protected_spans = {span};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "MozkeyとMozkey");
}

TEST(ZenzAdoptionPolicyTest, RejectsWhenRequiredOccurrenceIsMissing) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "もずきーともずきー";
  input.mozc_value = "MozkeyとMozkey";
  input.zenz_value = "Mozkeyだけ";

  ProtectedConversionSpan span = BuildSpan(
      "もずきー", "Mozkey",
      ProtectedConversionSpan::Tier::kIdentityCritical, true);
  span.required_occurrences = 2;
  input.protected_spans = {span};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
}

TEST(ZenzAdoptionPolicyTest, DoesNotRepairUserPreferredSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "わたなべ";
  input.mozc_value = "渡邊さんに送る";
  input.zenz_value = "渡辺さんへ送ります";
  input.protected_spans = {
      BuildSpan("わたなべ", "渡邊", ProtectedConversionSpan::Tier::kUserPreferred, false),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
}

TEST(ZenzAdoptionPolicyTest, DoesNotRepairShortAsciiSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "えーあい";
  input.mozc_value = "AIを使う";
  input.zenz_value = "エーアイを使います";
  input.protected_spans = {
      BuildSpan("えーあい", "AI", ProtectedConversionSpan::Tier::kIdentityCritical, true),
  };

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
}

TEST(ZenzAdoptionPolicyTest, RejectsUnsafeTransitionWithoutBaselineSegments) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょう";
  input.mozc_value = "東京";
  input.zenz_value = "Tokyo";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "東京");
  EXPECT_EQ(result.reason,
            "orthographic_transition_missing_baseline_segments");
}

TEST(ZenzAdoptionPolicyTest, RejectsUnsafeTransitionWithStaleBaselineSegments) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょう";
  input.mozc_value = "東京";
  input.zenz_value = "Tokyo";
  input.baseline_segments = {{"とうきょう", "大阪"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "東京");
  EXPECT_EQ(result.reason,
            "orthographic_transition_stale_baseline_segments");
}

TEST(ZenzAdoptionPolicyTest, AllowsJapaneseRewriteWithoutBaselineSegments) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "てんてき";
  input.mozc_value = "点滴";
  input.zenz_value = "天敵";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "天敵");
}

TEST(ZenzAdoptionPolicyTest, RepairsNewAlphabeticSurfacePerSegment) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょうにてんてき";
  input.mozc_value = "東京に点滴";
  input.zenz_value = "Tokyoに天敵";
  input.baseline_segments = {
      {"とうきょう", "東京"}, {"に", "に"}, {"てんてき", "点滴"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "東京に天敵");
  EXPECT_EQ(result.reason, "orthographic_transition_repaired");
}

TEST(ZenzAdoptionPolicyTest, PreservesExistingLatinSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "ぎっとはぶをつかう";
  input.mozc_value = "GitHubを使う";
  input.zenz_value = "GitHubを使用する";
  input.baseline_segments = {{"ぎっとはぶをつかう", "GitHubを使う"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "GitHubを使用する");
}

TEST(ZenzAdoptionPolicyTest, PreservesMozcSelectedLatinBaseline) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょうにいく";
  input.mozc_value = "Tokyoにいく";
  input.zenz_value = "Tokyoに行く";
  input.baseline_segments = {{"とうきょうにいく", "Tokyoにいく"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "Tokyoに行く");
}

TEST(ZenzAdoptionPolicyTest, RejectsUnprojectableNewAlphabeticSurface) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょうおおさか";
  input.mozc_value = "東京大阪";
  input.zenz_value = "TokyoOsaka";
  input.baseline_segments = {{"とうきょう", "東京"}, {"おおさか", "大阪"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "東京大阪");
  EXPECT_EQ(result.reason, "orthographic_transition_projection_failed");
}

TEST(ZenzAdoptionPolicyTest, AllowsUnprojectableJapaneseOnlyRewrite) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "とうきょうおおさか";
  input.mozc_value = "東京大阪";
  input.zenz_value = "首都関西";
  input.baseline_segments = {{"とうきょう", "東京"}, {"おおさか", "大阪"}};

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "首都関西");
}

TEST(ZenzAdoptionPolicyTest,
     AdoptsTypedAlphabeticSurfaceWhenScriptTransitionEnabled) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "ぎっとはぶにぷっしゅ";
  input.mozc_value = "ぎっとはぶにぷっしゅ";
  input.zenz_value = "GitHubにPush";
  input.baseline_segments = {
      {"ぎっとはぶ", "ぎっとはぶ"}, {"に", "に"}, {"ぷっしゅ", "ぷっしゅ"}};
  input.allow_script_transition = true;
  input.typed_raw_input = "githubnipush";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptAsIs);
  EXPECT_EQ(result.value, "GitHubにPush");
  EXPECT_EQ(result.reason, "accepted");
}

TEST(ZenzAdoptionPolicyTest,
     RevertsTypedAlphabeticSurfaceWhenScriptTransitionDisabled) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "ぎっとはぶにぷっしゅ";
  input.mozc_value = "ぎっとはぶにぷっしゅ";
  input.zenz_value = "GitHubにPush";
  input.baseline_segments = {
      {"ぎっとはぶ", "ぎっとはぶ"}, {"に", "に"}, {"ぷっしゅ", "ぷっしゅ"}};
  // Historical behaviour: no permission, and the candidate is reverted even
  // though the raw romaji would have licensed it.
  input.typed_raw_input = "githubnipush";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "ぎっとはぶにぷっしゅ");
  EXPECT_EQ(result.reason, "orthographic_transition_repaired");
}

TEST(ZenzAdoptionPolicyTest,
     RevertsUngroundedAlphabeticSurfaceWhenScriptTransitionEnabled) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "ぎっとはぶにぷっしゅ";
  input.mozc_value = "ぎっとはぶにぷっしゅ";
  input.zenz_value = "ZqxwにPush";
  input.baseline_segments = {
      {"ぎっとはぶ", "ぎっとはぶ"}, {"に", "に"}, {"ぷっしゅ", "ぷっしゅ"}};
  input.allow_script_transition = true;
  input.typed_raw_input = "githubnipush";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kAcceptWithRepair);
  EXPECT_EQ(result.value, "ぎっとはぶにPush");
  EXPECT_EQ(result.reason, "orthographic_transition_repaired");
}

TEST(ZenzAdoptionPolicyTest,
     RejectsUngroundedAlphabeticSurfaceWithoutBaselineSegments) {
  ZenzAdoptionPolicy policy;
  ZenzAdoptionInput input;
  input.key = "ぎっとはぶにぷっしゅ";
  input.mozc_value = "ぎっとはぶにぷっしゅ";
  input.zenz_value = "ZqxwにPush";
  input.allow_script_transition = true;
  input.typed_raw_input = "githubnipush";

  const ZenzAdoptionResult result = policy.Decide(input);
  EXPECT_EQ(result.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(result.value, "ぎっとはぶにぷっしゅ");
  EXPECT_EQ(result.reason,
            "orthographic_transition_missing_baseline_segments");
}

TEST(ZenzAdoptionPolicyTest,
     FusedRomanizationLeftoverIsNotABoundaryProtectedSurface) {
  // The gate session.cc (AddPreeditIdentityProtectedSpans) consults before it
  // turns a preedit ASCII identity surface into a protected span.  The letters
  // the kana table cannot consume stay in the composition fused to the kana
  // around them -- ぎてゅbにぷsh -- so they must not be protected as surfaces.
  EXPECT_FALSE(HasProtectedSurfaceBoundaryOccurrence("ぎてゅbにぷsh", "b"));
  EXPECT_FALSE(HasProtectedSurfaceBoundaryOccurrence("ぎてゅbにぷsh", "sh"));
  // A fragment glued to kanji is the same class of leftover: it is not a
  // boundary-complete surface on its own.
  EXPECT_FALSE(HasProtectedSurfaceBoundaryOccurrence("v3.2版を使う", "v3.2"));

  // Surfaces Mozc actually selected do have a boundary: a particle, punctuation,
  // a space, or an end of the value.
  EXPECT_TRUE(
      HasProtectedSurfaceBoundaryOccurrence("GitHubを使う", "GitHub"));
  EXPECT_TRUE(
      HasProtectedSurfaceBoundaryOccurrence("Mozkeyを使っています", "Mozkey"));
  EXPECT_TRUE(
      HasProtectedSurfaceBoundaryOccurrence("v3.2版を使う", "v3.2版"));
  EXPECT_TRUE(HasProtectedSurfaceBoundaryOccurrence("v3.2", "v3.2"));
}

TEST(ZenzAdoptionPolicyTest, DropsFusedLeftoverSpansBeforeDeciding) {
  ZenzAdoptionPolicy policy;
  const std::string mozc_value = "ぎてゅbにぷshしてほしいかな";

  ZenzAdoptionInput input;
  input.key = "ぎてゅbにぷshしてほしいかな";
  input.mozc_value = mozc_value;
  input.zenz_value = "GitHubにPushしてほしいかな";
  input.baseline_segments = {{"ぎてゅb", "ぎてゅb"},
                             {"に", "に"},
                             {"ぷsh", "ぷsh"},
                             {"してほしいかな", "してほしいかな"}};
  input.allow_script_transition = true;
  input.typed_raw_input = "githubnipushsitehosiikana";

  // Why the session has to drop these spans: "b" and "sh" are one and two
  // characters long, so the protected-surface gate can never repair them, and
  // the candidate is rejected with
  // "protected_user_dictionary_surface_boundary_changed" although it keeps every
  // letter the user typed.
  const std::vector<ProtectedConversionSpan> leftover_spans = {
      BuildSpan("", "b", ProtectedConversionSpan::Tier::kIdentityCritical,
                false),
      BuildSpan("", "sh", ProtectedConversionSpan::Tier::kIdentityCritical,
                false),
  };
  input.protected_spans = leftover_spans;
  const ZenzAdoptionResult rejected = policy.Decide(input);
  EXPECT_EQ(rejected.reason,
            "protected_user_dictionary_surface_boundary_changed");
  EXPECT_EQ(rejected.action, ZenzAdoptionResult::Action::kReject);
  EXPECT_EQ(rejected.value, mozc_value);

  // After the session keeps only boundary-complete surfaces, the very same
  // candidate is adopted with the English surface.
  input.protected_spans.clear();
  for (const ProtectedConversionSpan& span : leftover_spans) {
    if (HasProtectedSurfaceBoundaryOccurrence(mozc_value, span.value)) {
      input.protected_spans.push_back(span);
    }
  }
  EXPECT_TRUE(input.protected_spans.empty());

  const ZenzAdoptionResult adopted = policy.Decide(input);
  EXPECT_EQ(adopted.reason, "accepted");
  EXPECT_EQ(adopted.value, "GitHubにPushしてほしいかな");
  EXPECT_EQ(adopted.action, ZenzAdoptionResult::Action::kAcceptAsIs);
}

}  // namespace
}  // namespace mozc::session
