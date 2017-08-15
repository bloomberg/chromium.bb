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

#ifndef SimplifiedBackwardsTextIterator_h
#define SimplifiedBackwardsTextIterator_h

#include "core/editing/Position.h"
#include "core/editing/iterators/BackwardsTextBuffer.h"
#include "core/editing/iterators/FullyClippedStateStack.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "platform/heap/Heap.h"

namespace blink {

class LayoutText;

// Iterates through the DOM range, returning all the text, and 0-length
// boundaries at points where replaced elements break up the text flow. The text
// comes back in chunks so as to optimize for performance of the iteration.
template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT SimplifiedBackwardsTextIteratorAlgorithm {
  STACK_ALLOCATED();

 public:
  SimplifiedBackwardsTextIteratorAlgorithm(
      const PositionTemplate<Strategy>& start,
      const PositionTemplate<Strategy>& end,
      const TextIteratorBehavior& = TextIteratorBehavior());

  bool AtEnd() const { return !position_node_ || should_stop_; }
  void Advance();

  int length() const { return text_length_; }

  // Note: |characterAt()| returns characters in the reversed order, since
  // the iterator is backwards. For example, if the current text is "abc",
  // then |characterAt(0)| returns 'c'.
  UChar CharacterAt(unsigned index) const;

  Node* GetNode() const { return node_; }

  // Calculate the minimum |actualLength >= minLength| such that code units
  // with offset range [position, position + actualLength) are whole code
  // points. Prepend these code points to |output| and return |actualLength|.
  int CopyTextTo(BackwardsTextBuffer* output,
                 int position,
                 int min_length) const;
  int CopyTextTo(BackwardsTextBuffer* output, int position = 0) const;

  Node* StartContainer() const;
  int EndOffset() const;
  PositionTemplate<Strategy> StartPosition() const;
  PositionTemplate<Strategy> EndPosition() const;

  bool IsInTextSecurityMode() const;

 private:
  void Init(Node* start_node, Node* end_node, int start_offset, int end_offset);
  void ExitNode();
  bool HandleTextNode();
  LayoutText* HandleFirstLetter(int& start_offset, int& offset_in_node);
  bool HandleReplacedElement();
  bool HandleNonTextNode();
  void EmitCharacter(UChar, Node*, int start_offset, int end_offset);
  bool AdvanceRespectingRange(Node*);

  bool IsBetweenSurrogatePair(int position) const;

  // Prepend code units with offset range [position, position + copyLength)
  // to the output buffer.
  void CopyCodeUnitsTo(BackwardsTextBuffer* output,
                       int position,
                       int copy_length) const;

  // Current position, not necessarily of the text being returned, but position
  // as we walk through the DOM tree.
  Member<Node> node_;
  int offset_;
  bool handled_node_;
  bool handled_children_;
  FullyClippedStateStackAlgorithm<Strategy> fully_clipped_stack_;

  // End of the range.
  Member<Node> start_node_;
  int start_offset_;
  // Start of the range.
  Member<Node> end_node_;
  int end_offset_;

  // The current text and its position, in the form to be returned from the
  // iterator.
  Member<Node> position_node_;
  int position_start_offset_;
  int position_end_offset_;

  // We're interested in the range [m_textOffset, m_textOffset + m_textLength)
  // of m_textContainer.
  String text_container_;
  int text_offset_;
  int text_length_;

  // Used for whitespace characters that aren't in the DOM, so we can point at
  // them.
  UChar single_character_buffer_;

  // Whether m_node has advanced beyond the iteration range (i.e. m_startNode).
  bool have_passed_start_node_;

  // Should handle first-letter layoutObject in the next call to handleTextNode.
  bool should_handle_first_letter_;

  // Used when the iteration should stop if form controls are reached.
  bool stops_on_form_controls_;

  // Used when m_stopsOnFormControls is set to determine if the iterator should
  // keep advancing.
  bool should_stop_;

  // Used in pasting inside password field.
  bool emits_original_text_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy>;

using SimplifiedBackwardsTextIterator =
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy>;

}  // namespace blink

#endif  // SimplifiedBackwardsTextIterator_h
