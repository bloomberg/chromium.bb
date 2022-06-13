// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_H_
#define CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_H_

#include <string>
#include <vector>

#include "ui/gfx/range/range.h"

namespace chromeos {
namespace string_matching {

// TokenizedString takes a string and breaks it down into token words.
class TokenizedString {
 public:
  enum class Mode {
    // Break words into tokens at camel case and alpha/num boundaries.
    kCamelCase,
    // Break words into tokens at white space.
    kWords,
  };

  typedef std::vector<std::u16string> Tokens;
  typedef std::vector<gfx::Range> Mappings;

  explicit TokenizedString(const std::u16string& text,
                           Mode mode = Mode::kCamelCase);

  TokenizedString(const TokenizedString&) = delete;
  TokenizedString& operator=(const TokenizedString&) = delete;

  ~TokenizedString();

  const std::u16string& text() const { return text_; }
  const Tokens& tokens() const { return tokens_; }
  const Mappings& mappings() const { return mappings_; }

 private:
  void Tokenize();
  void TokenizeWords();

  // Input text.
  const std::u16string text_;

  // Broken down tokens and the index mapping of tokens in original string.
  Tokens tokens_;
  Mappings mappings_;
};

}  // namespace string_matching
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_STRING_MATCHING_TOKENIZED_STRING_H_
