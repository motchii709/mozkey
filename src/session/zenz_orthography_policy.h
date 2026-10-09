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

#ifndef MOZC_SESSION_ZENZ_ORTHOGRAPHY_POLICY_H_
#define MOZC_SESSION_ZENZ_ORTHOGRAPHY_POLICY_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc::session {

struct ZenzOrthographyDecision {
  bool allow = true;
  std::string reason = "accepted";
};

// Preserves the orthographic regime already selected by normal Mozc.  Zenz is
// allowed to change Japanese lexical interpretation, but the multiset of
// alphabetic lexical runs selected by normal Mozc is treated as an
// orthographic invariant.  ASCII technical tokens that contain alphabetic
// material (for example C++, GPT-5, UTF-8, HTTP/2, and Windows11) are also
// preserved exactly.  Numeric-only spans are deliberately outside this policy.
// Zenz alone may neither introduce, remove, nor mutate the protected surfaces.
//
// One additive, opt-in exception exists for mixed English/Japanese input.
// `allow_script_transition` mirrors config::Config::use_auto_language_switch
// (the user-visible "Auto" switch, default false).  When it is true, a Mozc
// surface that contains no ASCII letter at all (pure kana) may become ASCII
// letters, but only when every ASCII surface the candidate introduces was
// literally typed by the user as raw romaji, supplied in `typed_raw_input`
// (composer::Composer::GetRawString()).  Matching is case-insensitive and
// accepts a contiguous substring ("github" typed as raw romaji licenses the
// candidate run "GitHub") or a consonant-skeleton subsequence of the typed
// romaji: vowels are dropped on both sides and the remaining consonants must
// appear in the same order.  The consonant form is what makes a respelling work
// at all, because romanisation inserts epenthetic vowels that reorder letters
// (the word "GitHub" versus the typed "gittohabu", where ハブ is "habu"), while
// an invented spelling such as "Google" typed as "kanzidesu" stays rejected
// because its 'g' was never typed.  Consonant-skeleton matching also admits
// transliterations and short consonant subsets that the substring rule rejected,
// for example "Tokyo" from "toukyou"; that widening is the deliberately accepted
// cost of the relaxation and is recorded in orthography-subsequence-notes.md.
// Removal, mutation, and duplication of
// existing ASCII surfaces remain rejected, `typed_raw_input` is never enough on
// its own, and an empty `typed_raw_input` fails closed.
// The permission is scoped to ASCII letters: a candidate that introduces none
// (an ordinary Japanese rewrite such as くみ -> 組, or a fullwidth surface such
// as "ＡＢＣ") is decided by the historical comparison alone, so the flag cannot
// change its verdict.
// With the flag false the method behaves exactly as before.
class ZenzOrthographyPolicy {
 public:
  ZenzOrthographyDecision Evaluate(
      absl::string_view mozc_value, absl::string_view candidate_value,
      bool allow_script_transition = false,
      absl::string_view typed_raw_input = "") const;
};

}  // namespace mozc::session

#endif  // MOZC_SESSION_ZENZ_ORTHOGRAPHY_POLICY_H_
