/*
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef CharacterIterator_h
#define CharacterIterator_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/Forward.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "platform/heap/Heap.h"

namespace blink {

// Builds on the text iterator, adding a character position so we can walk one
// character at a time, or faster, as needed. Useful for searching.
template <typename Strategy>
class CORE_EXPORT CharacterIteratorAlgorithm {
  STACK_ALLOCATED();

 public:
  CharacterIteratorAlgorithm(
      const PositionTemplate<Strategy>& start,
      const PositionTemplate<Strategy>& end,
      const TextIteratorBehavior& = TextIteratorBehavior());
  explicit CharacterIteratorAlgorithm(
      const EphemeralRangeTemplate<Strategy>&,
      const TextIteratorBehavior& = TextIteratorBehavior());

  void Advance(int num_characters);

  bool AtBreak() const { return at_break_; }
  bool AtEnd() const { return text_iterator_.AtEnd(); }

  int length() const { return text_iterator_.length() - run_offset_; }
  UChar CharacterAt(unsigned index) const {
    return text_iterator_.CharacterAt(run_offset_ + index);
  }

  void CopyTextTo(ForwardsTextBuffer* output);

  int CharacterOffset() const { return offset_; }

  Document* OwnerDocument() const;
  Node* CurrentContainer() const;
  int StartOffset() const;
  int EndOffset() const;
  PositionTemplate<Strategy> StartPosition() const;
  PositionTemplate<Strategy> EndPosition() const;

  EphemeralRangeTemplate<Strategy> CalculateCharacterSubrange(int offset,
                                                              int length);

 private:
  void Initialize();

  int offset_;
  int run_offset_;
  bool at_break_;

  TextIteratorAlgorithm<Strategy> text_iterator_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    CharacterIteratorAlgorithm<EditingStrategy>;
using CharacterIterator = CharacterIteratorAlgorithm<EditingStrategy>;

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    CharacterIteratorAlgorithm<EditingInFlatTreeStrategy>;

CORE_EXPORT EphemeralRange CalculateCharacterSubrange(const EphemeralRange&,
                                                      int character_offset,
                                                      int character_count);

}  // namespace blink

#endif  // CharacterIterator_h
