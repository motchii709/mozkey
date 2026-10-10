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

#include <cstddef>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/util.h"

namespace mozc::session {
namespace {

// Keep version digits and technical connectors attached to Latin material so
// tokens such as GPT-5, C++, UTF-8, HTTP/2, and Windows11 are compared as one
// orthographic surface.  A token is retained only when it contains a letter,
// so ordinary numeric expressions remain outside this policy.
bool IsAsciiTechnicalTokenChar(const unsigned char c) {
  return (('0' <= c) && (c <= '9')) || (('A' <= c) && (c <= 'Z')) ||
         (('a' <= c) && (c <= 'z')) || c == '_' || c == '-' || c == '.' ||
         c == '+' || c == '#' || c == '/';
}

bool HasAsciiLetter(absl::string_view value) {
  for (const unsigned char c : value) {
    if ((('A' <= c) && (c <= 'Z')) || (('a' <= c) && (c <= 'z'))) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> ExtractAsciiTechnicalTokens(
    absl::string_view value) {
  std::vector<std::string> tokens;
  size_t start = absl::string_view::npos;

  auto flush = [&](size_t end) {
    if (start == absl::string_view::npos || end <= start) {
      start = absl::string_view::npos;
      return;
    }
    const absl::string_view token = value.substr(start, end - start);
    if (HasAsciiLetter(token)) {
      tokens.emplace_back(token);
    }
    start = absl::string_view::npos;
  };

  for (size_t i = 0; i < value.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (IsAsciiTechnicalTokenChar(c)) {
      if (start == absl::string_view::npos) {
        start = i;
      }
    } else {
      flush(i);
    }
  }
  flush(value.size());
  return tokens;
}

std::vector<std::string> ExtractAlphabetRuns(absl::string_view value) {
  std::vector<std::string> runs;
  std::string current;

  for (ConstChar32Iterator iter(value); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();
    if (Util::GetScriptType(codepoint) == Util::ALPHABET) {
      Util::CodepointToUtf8Append(codepoint, &current);
      continue;
    }

    if (!current.empty()) {
      runs.push_back(current);
      current.clear();
    }
  }

  if (!current.empty()) {
    runs.push_back(current);
  }
  return runs;
}

// The baseline filter below needs the "is this ASCII fused to its
// surroundings?" test that the adoption policy uses to tell a Mozc-selected
// ASCII surface apart from a fragment glued to kana.  This is the lower layer
// (session/zenz_adoption_policy.h depends on this header), so the predicate is
// repeated here instead of shared; keep it in step with
// session/zenz_adoption_policy.cc's IsJapaneseLetterLike /
// IsProtectedSurfaceBoundaryChar.
bool IsAsciiWordLikeCodepoint(char32_t c) {
  return (U'0' <= c && c <= U'9') || (U'A' <= c && c <= U'Z') ||
         (U'a' <= c && c <= U'z') || c == U'_' || c == U'-' || c == U'.' ||
         c == U'+' || c == U'#';
}

bool IsJapaneseLetterLikeCodepoint(char32_t c) {
  return (0x3040 <= c && c <= 0x309F) ||  // Hiragana
         (0x30A0 <= c && c <= 0x30FF) ||  // Katakana
         (0x3400 <= c && c <= 0x9FFF) ||  // CJK Unified Ideographs
         (0xF900 <= c && c <= 0xFAFF) ||  // CJK Compatibility Ideographs
         (0xFF66 <= c && c <= 0xFF9D);    // Halfwidth Katakana
}

bool IsJapaneseParticleBoundaryCodepoint(char32_t c) {
  switch (c) {
    case U'の':
    case U'は':
    case U'を':
    case U'に':
    case U'で':
    case U'へ':
    case U'と':
    case U'が':
    case U'も':
    case U'や':
    case U'か':
      return true;
    default:
      return false;
  }
}

bool IsPunctuationOrSpaceBoundaryCodepoint(char32_t c) {
  if (c <= 0x20) {
    return true;
  }
  switch (c) {
    case U'、':
    case U'。':
    case U'，':
    case U'．':
    case U'・':
    case U'「':
    case U'」':
    case U'『':
    case U'』':
    case U'（':
    case U'）':
    case U'(':
    case U')':
    case U'[':
    case U']':
    case U'{':
    case U'}':
      return true;
    default:
      return false;
  }
}

bool IsProtectedSurfaceBoundaryCodepoint(char32_t c) {
  if (IsPunctuationOrSpaceBoundaryCodepoint(c) ||
      IsJapaneseParticleBoundaryCodepoint(c)) {
    return true;
  }
  return !IsJapaneseLetterLikeCodepoint(c) && !IsAsciiWordLikeCodepoint(c);
}

// The ALPHABET runs of `value` that are boundary-complete: every run whose
// neighbours in `value` are Japanese particles, punctuation, spaces, or an end
// of the string.  A run with a non-particle Japanese letter or an ASCII word
// character beside it was fused to its surroundings by the composer, which is
// exactly what a romanisation leftover looks like: the kana table cannot consume
// a consonant such as the "b" of "github" or the "sh" of "push", so it stays
// inside ぎてゅbにぷsh as literal half-width ASCII and shows up in the Mozc
// value -- but it was never an ASCII surface Mozc selected.  The candidate side
// keeps using every run, because a candidate surface is judged by where its
// letters came from, not by what surrounds them.
std::vector<std::string> ExtractBoundaryCompleteAlphabetRuns(
    absl::string_view value) {
  std::vector<std::string> runs;
  std::string current;
  bool pending = false;
  bool left_is_boundary = true;
  char32_t previous = 0;
  bool has_previous = false;

  const auto flush = [&](char32_t right, bool has_right) {
    if (!pending) {
      return;
    }
    if (left_is_boundary &&
        (!has_right || IsProtectedSurfaceBoundaryCodepoint(right))) {
      runs.push_back(current);
    }
    current.clear();
    pending = false;
  };

  for (ConstChar32Iterator iter(value); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();
    if (Util::GetScriptType(codepoint) == Util::ALPHABET) {
      if (!pending) {
        pending = true;
        left_is_boundary =
            !has_previous || IsProtectedSurfaceBoundaryCodepoint(previous);
      }
      Util::CodepointToUtf8Append(codepoint, &current);
    } else {
      flush(codepoint, /*has_right=*/true);
    }
    previous = codepoint;
    has_previous = true;
  }
  flush(0, /*has_right=*/false);
  return runs;
}

bool StringMultisetsMatch(const std::vector<std::string>& baseline_values,
                          const std::vector<std::string>& candidate_values) {
  if (baseline_values.size() != candidate_values.size()) {
    return false;
  }

  std::vector<bool> consumed(baseline_values.size(), false);

  for (const std::string& candidate_value : candidate_values) {
    bool found = false;
    for (size_t i = 0; i < baseline_values.size(); ++i) {
      if (!consumed[i] && baseline_values[i] == candidate_value) {
        consumed[i] = true;
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

// ASCII-only case folding.  Non-ASCII bytes are copied verbatim, which is
// harmless because raw romaji input for a Japanese composition is ASCII, so a
// non-ASCII surface can never match it.
std::string ToLowerAscii(absl::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (const char c : value) {
    const unsigned char u = static_cast<unsigned char>(c);
    lowered.push_back((('A' <= u) && (u <= 'Z'))
                          ? static_cast<char>(u - 'A' + 'a')
                          : c);
  }
  return lowered;
}

// ASCII vowels are dropped before the order comparison because Japanese
// romanisation inserts epenthetic vowels that shuffle the letters: ギットハブ is
// typed "gittohabu" (ハブ = "habu", so 'b' precedes 'u'), while the word itself
// is "GitHub" (the 'u' precedes the 'b').  The consonant order, not the full
// letter order, is the part a respelling preserves.  A surface made only of
// vowels has an empty skeleton and falls back to the contiguous rule.
bool IsAsciiVowel(const char c) {
  return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o';
}

std::string ConsonantSkeleton(absl::string_view lowered) {
  std::string skeleton;
  skeleton.reserve(lowered.size());
  for (const char c : lowered) {
    if (!IsAsciiVowel(c)) {
      skeleton.push_back(c);
    }
  }
  return skeleton;
}

// True when the consonant skeleton of `lowered_surface` occurs in the consonant
// skeleton of `lowered_raw_input` in order but not necessarily contiguously.
// Both arguments must already be ASCII case-folded.
bool IsConsonantSkeletonSubsequenceOfTypedRaw(
    absl::string_view lowered_raw_input, absl::string_view lowered_surface) {
  const std::string needle = ConsonantSkeleton(lowered_surface);
  if (needle.empty()) {
    return false;
  }

  const std::string haystack = ConsonantSkeleton(lowered_raw_input);
  size_t next = 0;
  for (const char c : haystack) {
    if (c == needle[next] && ++next == needle.size()) {
      return true;
    }
  }
  return false;
}

// True when `surface` could plausibly have been typed by the user.
// `lowered_raw_input` is the case-folded composer raw romaji.  This is the
// grounding rule for the mixed-input permission, and it accepts two shapes:
//
//   1. a contiguous substring of the raw keystrokes -- the historical rule,
//      tested first so every previously licensed surface keeps its old path;
//   2. a consonant-skeleton subsequence of them -- every consonant of the
//      surface was typed, in order, ignoring the vowels that romanisation
//      inserted.
//
// The second shape exists because a Japanese respelling of a Latin word is not
// letter-order preserving: the word "GitHub" reverses the 'u' and the 'b' of the
// typed "gittohabu" (ハブ = "habu").  The consonant order is what still blocks
// arbitrary Latin invention while admitting a respelling: "Google" against
// "kanzidesu" has no 'g' at all.
//
// Deliberate residual risk: dropping vowels and contiguity also licenses
// transliterations and short consonant subsets of the romaji, for example
// "Tokyo" from "toukyou".  That widening is the accepted cost of the relaxation
// and is recorded in orthography-subsequence-notes.md.  Tightening this to a
// per-segment raw substring (composer::Composer::GetRawSubString) is future
// work.
bool IsTypedRawSurface(absl::string_view lowered_raw_input,
                       absl::string_view surface) {
  if (lowered_raw_input.empty() || surface.empty()) {
    return false;
  }

  const std::string lowered_surface = ToLowerAscii(surface);
  if (lowered_raw_input.find(lowered_surface) != absl::string_view::npos) {
    return true;
  }
  return IsConsonantSkeletonSubsequenceOfTypedRaw(lowered_raw_input,
                                                  lowered_surface);
}

bool AllSurfacesAreTypedRaw(absl::string_view lowered_raw_input,
                            const std::vector<std::string>& surfaces) {
  for (const std::string& surface : surfaces) {
    if (!IsTypedRawSurface(lowered_raw_input, surface)) {
      return false;
    }
  }
  return true;
}

}  // namespace

ZenzOrthographyDecision ZenzOrthographyPolicy::Evaluate(
    absl::string_view mozc_value, absl::string_view candidate_value,
    bool allow_script_transition, absl::string_view typed_raw_input) const {
  if (!Util::IsValidUtf8(mozc_value) || !Util::IsValidUtf8(candidate_value)) {
    ZenzOrthographyDecision decision;
    decision.allow = false;
    decision.reason = "invalid_utf8";
    return decision;
  }

  if (mozc_value == candidate_value) {
    return {};
  }

  const std::vector<std::string> candidate_runs =
      ExtractAlphabetRuns(candidate_value);
  // Baseline alphabet runs that normal Mozc already selected.  Under the
  // mixed-input permission only boundary-complete runs count as surfaces: a run
  // the composer fused to the kana beside it (or to a neighbouring ASCII word
  // character) is a romanisation leftover, and counting it closed the whole
  // permission for compositions such as ぎてゅbにぷsh..., so the model's
  // GitHubにPush... was refused with "alphabetic_surface_changed".  A
  // boundary-complete run is a real surface and still blocks.  With the flag off
  // the unfiltered list is used verbatim, so the historical path is
  // byte-identical.
  const std::vector<std::string> baseline_runs =
      allow_script_transition ? ExtractBoundaryCompleteAlphabetRuns(mozc_value)
                              : ExtractAlphabetRuns(mozc_value);

  // The mixed-input permission is deliberately narrow.  It applies only when
  // the Mozc surface carries no ASCII surface of its own, so it can never
  // remove, mutate, or duplicate a surface that normal Mozc already selected;
  // it only lets a kana segment surface letters the user actually typed.  An
  // empty raw string fails closed.  The baseline technical-token list below is
  // deliberately not filtered the same way (a fused leftover is a technical
  // token too); the token check can still only accept candidate tokens that the
  // typed romaji grounds.  With the flag false this is all inert and behaviour
  // is byte-identical to before.
  const bool script_transition_allowed =
      allow_script_transition && baseline_runs.empty() &&
      !typed_raw_input.empty();
  const std::string lowered_raw_input =
      script_transition_allowed ? ToLowerAscii(typed_raw_input)
                                : std::string();

  if (!StringMultisetsMatch(baseline_runs, candidate_runs) &&
      !(script_transition_allowed &&
        AllSurfacesAreTypedRaw(lowered_raw_input, candidate_runs))) {
    ZenzOrthographyDecision decision;
    decision.allow = false;
    decision.reason = "alphabetic_surface_changed";
    return decision;
  }

  const std::vector<std::string> baseline_tokens =
      ExtractAsciiTechnicalTokens(mozc_value);
  const std::vector<std::string> candidate_tokens =
      ExtractAsciiTechnicalTokens(candidate_value);
  if (!StringMultisetsMatch(baseline_tokens, candidate_tokens) &&
      !(script_transition_allowed &&
        AllSurfacesAreTypedRaw(lowered_raw_input, candidate_tokens))) {
    ZenzOrthographyDecision decision;
    decision.allow = false;
    decision.reason = "technical_token_surface_changed";
    return decision;
  }

  return {};
}

}  // namespace mozc::session
