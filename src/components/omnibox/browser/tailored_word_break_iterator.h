// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_

#include "base/i18n/break_iterator.h"

// Breaks on an underscore. Otherwise, it behaves like its parent class with
// BreakIterator::BREAK_WORD.
class TailoredWordBreakIterator : public base::i18n::BreakIterator {
 public:
  TailoredWordBreakIterator(const base::StringPiece16& str,
                            base::i18n::BreakIterator::BreakType break_type);

  ~TailoredWordBreakIterator();

  bool Advance();
  bool IsWord() const;
  // Returns characters between |prev_| and |pos_| if |underscore_word_| is not
  // empty. Otherwise returns the normal BreakIterator-determined current word.
  base::StringPiece16 GetStringPiece() const;
  base::string16 GetString() const;
  size_t prev() const;
  size_t pos() const;

 private:
  // Returns true if we processing a word with underscores (i.e., |pos| points
  // to a valid position in |underscore_word_|).
  bool HasUnderscoreWord() const;
  // Updates |prev_| and |pos_| considering underscore.
  void AdvanceInUnderscoreWord();
  // |prev_| and |pos_| are indices to |underscore_word_|.
  size_t prev_, pos_;
  // Set if BreakIterator::GetStringPiece() contains '_', otherwise it's empty.
  base::StringPiece16 underscore_word_;

  DISALLOW_COPY_AND_ASSIGN(TailoredWordBreakIterator);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAILORED_WORD_BREAK_ITERATOR_H_
