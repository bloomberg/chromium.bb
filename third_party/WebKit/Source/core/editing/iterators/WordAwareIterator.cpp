/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/iterators/WordAwareIterator.h"

namespace blink {

WordAwareIterator::WordAwareIterator(const Position& start, const Position& end)
    // So we consider the first chunk from the text iterator.
    : did_look_ahead_(true), text_iterator_(start, end) {
  Advance();  // Get in position over the first chunk of text.
}

WordAwareIterator::~WordAwareIterator() {}

// FIXME: Performance could be bad for huge spans next to each other that don't
// fall on word boundaries.

void WordAwareIterator::Advance() {
  buffer_.Clear();

  // If last time we did a look-ahead, start with that looked-ahead chunk now
  if (!did_look_ahead_) {
    DCHECK(!text_iterator_.AtEnd());
    text_iterator_.Advance();
  }
  did_look_ahead_ = false;

  // Go to next non-empty chunk.
  while (!text_iterator_.AtEnd() && !text_iterator_.length())
    text_iterator_.Advance();

  if (text_iterator_.AtEnd())
    return;

  while (1) {
    // If this chunk ends in whitespace we can just use it as our chunk.
    if (IsSpaceOrNewline(
            text_iterator_.CharacterAt(text_iterator_.length() - 1)))
      return;

    // If this is the first chunk that failed, save it in m_buffer before look
    // ahead.
    if (buffer_.IsEmpty())
      text_iterator_.CopyTextTo(&buffer_);

    // Look ahead to next chunk. If it is whitespace or a break, we can use the
    // previous stuff
    text_iterator_.Advance();
    if (text_iterator_.AtEnd() || !text_iterator_.length() ||
        IsSpaceOrNewline(text_iterator_.GetText().CharacterAt(0))) {
      did_look_ahead_ = true;
      return;
    }

    // Start gobbling chunks until we get to a suitable stopping point
    text_iterator_.CopyTextTo(&buffer_);
  }
}

int WordAwareIterator::length() const {
  if (!buffer_.IsEmpty())
    return buffer_.Size();
  return text_iterator_.length();
}

String WordAwareIterator::Substring(unsigned position, unsigned length) const {
  if (!buffer_.IsEmpty())
    return String(buffer_.Data() + position, length);
  return text_iterator_.GetText().Substring(position, length);
}

UChar WordAwareIterator::CharacterAt(unsigned index) const {
  if (!buffer_.IsEmpty())
    return buffer_[index];
  return text_iterator_.GetText().CharacterAt(index);
}

}  // namespace blink
