// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_matching/tokenized_string.h"

#include <stddef.h>

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "chrome/common/string_matching/term_break_iterator.h"

using base::i18n::BreakIterator;

TokenizedString::TokenizedString(const base::string16& text) : text_(text) {
  Tokenize();
}

TokenizedString::~TokenizedString() = default;

void TokenizedString::Tokenize() {
  BreakIterator break_iter(text_, BreakIterator::BREAK_WORD);
  if (!break_iter.Init()) {
    NOTREACHED() << "BreakIterator init failed"
                 << ", text=\"" << text_ << "\"";
    return;
  }

  while (break_iter.Advance()) {
    if (!break_iter.IsWord())
      continue;

    const base::string16 word(break_iter.GetString());
    const size_t word_start = break_iter.prev();
    TermBreakIterator term_iter(word);
    while (term_iter.Advance()) {
      tokens_.emplace_back(base::i18n::ToLower(term_iter.GetCurrentTerm()));
      mappings_.emplace_back(word_start + term_iter.prev(),
                             word_start + term_iter.pos());
    }
  }
}
