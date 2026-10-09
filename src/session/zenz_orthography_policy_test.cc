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

#include "session/zenz_orthography_policy.h"

#include "testing/gunit.h"

namespace mozc::session {
namespace {

TEST(ZenzOrthographyPolicyTest, RejectsJapaneseSurfaceRomanization) {
  ZenzOrthographyPolicy policy;

  const ZenzOrthographyDecision decision = policy.Evaluate("東京", "Tokyo");

  EXPECT_FALSE(decision.allow);
  EXPECT_EQ(decision.reason, "alphabetic_surface_changed");
}

TEST(ZenzOrthographyPolicyTest, AllowsExistingLatinSurface) {
  ZenzOrthographyPolicy policy;

  EXPECT_TRUE(policy.Evaluate("GitHubを使う", "GitHubを使用する").allow);
  EXPECT_TRUE(policy.Evaluate("AIについて", "AIに関して").allow);
  EXPECT_TRUE(policy.Evaluate("Tokyoにいく", "Tokyoに行く").allow);
}

TEST(ZenzOrthographyPolicyTest, RejectsMutationOfExistingLatinSurface) {
  ZenzOrthographyPolicy policy;

  EXPECT_FALSE(policy.Evaluate("GitHubを使う", "GitLabを使う").allow);
  EXPECT_FALSE(policy.Evaluate("Tokyoに行く", "TOKYOに行く").allow);
}

TEST(ZenzOrthographyPolicyTest, RejectsTechnicalTokenMutation) {
  ZenzOrthographyPolicy policy;

  EXPECT_FALSE(policy.Evaluate("C++で書く", "C#で書く").allow);
  EXPECT_FALSE(policy.Evaluate("GPT-5を使う", "GPT5を使う").allow);
  EXPECT_FALSE(policy.Evaluate("UTF-8で保存", "UTF8で保存").allow);
  EXPECT_FALSE(policy.Evaluate("HTTP/2で接続", "HTTP/3で接続").allow);
  EXPECT_FALSE(policy.Evaluate("Windows11を使う", "Windows12を使う").allow);
}

TEST(ZenzOrthographyPolicyTest, DoesNotFreezeUnrelatedNumbers) {
  ZenzOrthographyPolicy policy;

  EXPECT_TRUE(policy.Evaluate("AIを2回使う", "AIを3回使う").allow);
}

TEST(ZenzOrthographyPolicyTest, RejectsRemovalOfExistingLatinSurface) {
  ZenzOrthographyPolicy policy;

  EXPECT_FALSE(policy.Evaluate("GitHubを使う", "ギットハブを使う").allow);
}

TEST(ZenzOrthographyPolicyTest, RejectsAdditionalAlphabetOccurrence) {
  ZenzOrthographyPolicy policy;

  EXPECT_FALSE(policy.Evaluate("AIを使う", "AIとAIを使う").allow);
}

TEST(ZenzOrthographyPolicyTest, AllowsJapaneseOnlyRewrite) {
  ZenzOrthographyPolicy policy;

  EXPECT_TRUE(policy.Evaluate("彼は点滴です", "彼は天敵です").allow);
}

TEST(ZenzOrthographyPolicyTest,
     AcceptsTypedAlphabeticSurfaceWhenScriptTransitionEnabled) {
  ZenzOrthographyPolicy policy;

  EXPECT_TRUE(policy
                  .Evaluate("ぎっとはぶにぷっしゅ", "GitHubにPush",
                            /*allow_script_transition=*/true, "githubnipush")
                  .allow);
  // The historical contiguous rule is unchanged, including this exact pair:
  // both runs are substrings of the typed romaji.
  EXPECT_TRUE(policy
                  .Evaluate("ぎっとはぶにぷっしゅ", "GitHubにpush",
                            /*allow_script_transition=*/true, "githubnipush")
                  .allow);
  // The permission never stands alone: without the typed raw romaji the same
  // transition is still rejected.
  EXPECT_FALSE(policy
                   .Evaluate("ぎっとはぶにぷっしゅ", "GitHubにPush",
                             /*allow_script_transition=*/true, "")
                   .allow);
}

TEST(ZenzOrthographyPolicyTest,
     RejectsUngroundedAlphabeticSurfaceWhenScriptTransitionEnabled) {
  ZenzOrthographyPolicy policy;

  const ZenzOrthographyDecision decision =
      policy.Evaluate("とうきょう", "Zqxw",
                      /*allow_script_transition=*/true, "toukyou");
  EXPECT_FALSE(decision.allow);
  EXPECT_EQ(decision.reason, "alphabetic_surface_changed");
}

TEST(ZenzOrthographyPolicyTest,
     KeepsRejectingAlphabeticSurfaceWhenScriptTransitionDisabled) {
  ZenzOrthographyPolicy policy;

  EXPECT_FALSE(policy
                   .Evaluate("ぎっとはぶにぷっしゅ", "GitHubにPush",
                             /*allow_script_transition=*/false, "githubnipush")
                   .allow);
  // A surface that only the subsequence rule could ground is rejected just as
  // firmly while the switch is off.
  EXPECT_FALSE(policy
                   .Evaluate("ぷっし", "push",
                             /*allow_script_transition=*/false,
                             "gittohabunipusshishitehosiikana")
                   .allow);
  // The defaulted arguments keep the historical two-argument call contract.
  EXPECT_FALSE(policy.Evaluate("ぎっとはぶにぷっしゅ", "GitHubにPush").allow);
}

TEST(ZenzOrthographyPolicyTest,
     DoesNotWeakenLatinSurfaceGuardsWhenScriptTransitionEnabled) {
  ZenzOrthographyPolicy policy;

  // Removal, mutation, and duplication of Mozc-selected ASCII surfaces stay
  // rejected even when the typed romaji contains these very letters.
  EXPECT_FALSE(policy
                   .Evaluate("GitHubを使う", "ギットハブを使う",
                             /*allow_script_transition=*/true, "github")
                   .allow);
  EXPECT_FALSE(policy
                   .Evaluate("GitHubを使う", "GitLabを使う",
                             /*allow_script_transition=*/true, "githubgitlab")
                   .allow);
  EXPECT_FALSE(policy
                   .Evaluate("AIを使う", "AIとAIを使う",
                             /*allow_script_transition=*/true, "aiaiai")
                   .allow);
}

TEST(ZenzOrthographyPolicyTest,
     AcceptsConsonantSkeletonGroundedSurfaceWhenScriptTransitionEnabled) {
  ZenzOrthographyPolicy policy;

  // "push" is not a contiguous substring of the typed romaji, which spells it
  // "pusshi", but its consonant skeleton p-s-h is an in-order subsequence of the
  // typed skeleton p-s-s-h.
  EXPECT_TRUE(policy
                  .Evaluate("ぷっし", "push",
                            /*allow_script_transition=*/true,
                            "gittohabunipusshishitehosiikana")
                  .allow);
  // Pins the predicate itself rather than a plausible input: the skeleton of
  // "KdE" (k-d) is an in-order subsequence of "kanzidesu" (k-n-z-d-s).
  EXPECT_TRUE(policy
                  .Evaluate("かんじです", "KdE",
                            /*allow_script_transition=*/true, "kanzidesu")
                  .allow);
}

TEST(ZenzOrthographyPolicyTest,
     RejectsConsonantSkeletonWithALetterTheUserNeverTyped) {
  ZenzOrthographyPolicy policy;

  // "kanzidesu" has no 'g' at all, so the consonant rule cannot ground an
  // invented Latin word, whatever order its letters are in.
  const ZenzOrthographyDecision decision =
      policy.Evaluate("かんじです", "Google",
                      /*allow_script_transition=*/true, "kanzidesu");
  EXPECT_FALSE(decision.allow);
  EXPECT_EQ(decision.reason, "alphabetic_surface_changed");
  // Nor can it ground a single letter that was never typed.
  EXPECT_FALSE(policy
                   .Evaluate("かんじです", "Q",
                             /*allow_script_transition=*/true, "kanzidesu")
                   .allow);
  // An empty raw string still fails closed for a skeleton-eligible surface.
  EXPECT_FALSE(policy
                   .Evaluate("かんじです", "KDE",
                             /*allow_script_transition=*/true, "")
                   .allow);
}

TEST(ZenzOrthographyPolicyTest,
     RejectsConsonantSkeletonWithDigitsOrConnectorsTheUserNeverTyped) {
  ZenzOrthographyPolicy policy;

  // The letters-only run "GPT" is grounded by "gptgoo", but the technical
  // token "GPT-5" is not: neither '-' nor '5' was typed, so the token rule is
  // the one that rejects.
  const ZenzOrthographyDecision decision =
      policy.Evaluate("じーぴーてぃーご", "GPT-5",
                      /*allow_script_transition=*/true, "gptgoo");
  EXPECT_FALSE(decision.allow);
  EXPECT_EQ(decision.reason, "technical_token_surface_changed");
  // With romaji whose skeleton does not contain g-p-t in order, the run rule is
  // the one that rejects.
  EXPECT_FALSE(policy
                   .Evaluate("じーぴーてぃーご", "GPT-5",
                             /*allow_script_transition=*/true, "jiipitiigoo")
                   .allow);
}

TEST(ZenzOrthographyPolicyTest, HasNoMinimumAlphabetRunLength) {
  ZenzOrthographyPolicy policy;

  // Pre-existing behaviour that this change does not alter: the policy
  // constrains where letters came from, not how many there are, so a single
  // typed letter is licensed.  There is no minimum run length here.
  EXPECT_TRUE(policy
                  .Evaluate("かんじです", "K",
                            /*allow_script_transition=*/true, "kanzidesu")
                  .allow);
}

TEST(ZenzOrthographyPolicyTest,
     RejectsSurfaceWhoseConsonantsWereTypedOutOfOrder) {
  ZenzOrthographyPolicy policy;

  // The skeleton must be consumed in order: "Bath" (b-t-h) cannot be grounded
  // by "gittohabu" (g-t-t-h-b) because nothing follows its 'b'.
  EXPECT_FALSE(policy
                   .Evaluate("ぎっとはぶ", "Bath",
                             /*allow_script_transition=*/true, "gittohabu")
                   .allow);
  // The same romaji does ground the respelling once the vowels are ignored.
  EXPECT_TRUE(policy
                  .Evaluate("ぎっとはぶ", "Hub",
                            /*allow_script_transition=*/true, "gittohabu")
                  .allow);
}

TEST(ZenzOrthographyPolicyTest,
     NowAcceptsATransliterationWhoseConsonantsWereTypedInOrder) {
  ZenzOrthographyPolicy policy;

  // Regression introduced by consonant-skeleton grounding and consciously
  // accepted: "tokyo" (t-k-y) and "toukyou" (t-k-y) share a consonant skeleton,
  // so the transliteration that the substring rule deliberately rejected is now
  // licensed.  The invented spelling beside it stays rejected.
  EXPECT_TRUE(policy
                  .Evaluate("とうきょう", "Tokyo",
                            /*allow_script_transition=*/true, "toukyou")
                  .allow);
  EXPECT_FALSE(policy
                   .Evaluate("とうきょう", "Zqxw",
                             /*allow_script_transition=*/true, "toukyou")
                   .allow);
}

TEST(ZenzOrthographyPolicyTest, AcceptsTheHeadlinePhoneticRespelling) {
  ZenzOrthographyPolicy policy;

  // The headline mixed-input example: typing
  // "gittohabunipusshishitehosiikana" (ぎっとはぶにぷっししてほしいかな) and asking
  // Zenz for "GitHubにpushしてほしいかな".  "GitHub" is grounded because its
  // consonant skeleton g-t-h-b is an in-order subsequence of the typed skeleton
  // g-t-t-h-b-..., which no full-letter rule can see: ハブ is typed "habu", so the
  // typed letters are h-a-b-u while the word needs u-b.  "push" is grounded by
  // the same rule (p-s-h inside p-s-s-h), and the candidate as a whole is
  // therefore adopted.
  EXPECT_TRUE(policy
                  .Evaluate("ぎっとはぶにぷっししてほしいかな",
                            "GitHubにpushしてほしいかな",
                            /*allow_script_transition=*/true,
                            "gittohabunipusshishitehosiikana")
                  .allow);
  EXPECT_TRUE(policy
                  .Evaluate("ぎっとはぶにぷっししてほしいかな",
                            "pushしてほしいかな",
                            /*allow_script_transition=*/true,
                            "gittohabunipusshishitehosiikana")
                  .allow);
}

TEST(ZenzOrthographyPolicyTest,
     IgnoresScriptTransitionWhenCandidateHasNoAsciiLetter) {
  ZenzOrthographyPolicy policy;

  // くみ -> 組 is a Japanese-only rewrite, so the mixed-input permission never
  // applies to it.  With the switch off the historical comparison accepts it
  // (see AllowsJapaneseOnlyRewrite) and the switch must not change that: the
  // verdict is the same, accepted, in both states even though the typed raw
  // romaji is available.
  const ZenzOrthographyDecision kana_rewrite_enabled =
      policy.Evaluate("くみ", "組", /*allow_script_transition=*/true, "kumi");
  const ZenzOrthographyDecision kana_rewrite_disabled =
      policy.Evaluate("くみ", "組", /*allow_script_transition=*/false, "kumi");
  EXPECT_TRUE(kana_rewrite_disabled.allow);
  EXPECT_EQ(kana_rewrite_enabled.allow, kana_rewrite_disabled.allow);
  EXPECT_EQ(kana_rewrite_enabled.reason, kana_rewrite_disabled.reason);

  // The permission is ASCII-only, so it cannot license a non-ASCII "letter"
  // surface either: fullwidth ＡＢＣ is an ALPHABET run without a single ASCII
  // letter, and it is rejected exactly as it is with the switch off.
  const ZenzOrthographyDecision fullwidth_enabled = policy.Evaluate(
      "くみ", "ＡＢＣ", /*allow_script_transition=*/true, "abc");
  const ZenzOrthographyDecision fullwidth_disabled = policy.Evaluate(
      "くみ", "ＡＢＣ", /*allow_script_transition=*/false, "abc");
  EXPECT_FALSE(fullwidth_disabled.allow);
  EXPECT_EQ(fullwidth_enabled.allow, fullwidth_disabled.allow);
  EXPECT_EQ(fullwidth_enabled.reason, fullwidth_disabled.reason);
}

TEST(ZenzOrthographyPolicyTest, LicensesOnlyTheAsciiLettersOfAMixedCandidate) {
  ZenzOrthographyPolicy policy;

  // The permission licenses the grounded ASCII run ("GitHub" from "gittohabu")
  // and nothing else; the Japanese part of the same candidate (くみ -> 組) is
  // left to the historical comparison, which accepts it with the switch off as
  // well.
  EXPECT_TRUE(policy
                  .Evaluate("くみとぎっとはぶ", "組とGitHub",
                            /*allow_script_transition=*/true,
                            "kumitogittohabu")
                  .allow);

  // The same mixed shape with an ASCII run the typed romaji does not ground is
  // still rejected: the permission never widens beyond typed ASCII letters.
  EXPECT_FALSE(policy
                   .Evaluate("くみとぎっとはぶ", "組とZqxw",
                             /*allow_script_transition=*/true,
                             "kumitogittohabu")
                   .allow);
}

}  // namespace
}  // namespace mozc::session
