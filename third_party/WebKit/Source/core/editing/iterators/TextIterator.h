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

#ifndef TextIterator_h
#define TextIterator_h

#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FindOptions.h"
#include "core/editing/iterators/FullyClippedStateStack.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "core/editing/iterators/TextIteratorTextState.h"
#include "platform/heap/Handle.h"

namespace blink {

class TextIteratorTextNodeHandler;

CORE_EXPORT String
PlainText(const EphemeralRange&,
          const TextIteratorBehavior& = TextIteratorBehavior());

String PlainText(const EphemeralRangeInFlatTree&,
                 const TextIteratorBehavior& = TextIteratorBehavior());

// Iterates through the DOM range, returning all the text, and 0-length
// boundaries at points where replaced elements break up the text flow.  The
// text comes back in chunks so as to optimize for performance of the iteration.

template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT TextIteratorAlgorithm {
  STACK_ALLOCATED();

 public:
  // [start, end] indicates the document range that the iteration should take
  // place within (both ends inclusive).
  TextIteratorAlgorithm(const PositionTemplate<Strategy>& start,
                        const PositionTemplate<Strategy>& end,
                        const TextIteratorBehavior& = TextIteratorBehavior());

  // Same behavior as previous constructor but takes an EphemeralRange instead
  // of two Positions
  TextIteratorAlgorithm(const EphemeralRangeTemplate<Strategy>&,
                        const TextIteratorBehavior& = TextIteratorBehavior());

  ~TextIteratorAlgorithm();

  bool AtEnd() const { return !text_state_->PositionNode() || should_stop_; }
  void Advance();
  bool IsInsideAtomicInlineElement() const;
  bool IsInTextSecurityMode() const;

  EphemeralRangeTemplate<Strategy> Range() const;
  Node* GetNode() const;

  Document* OwnerDocument() const;
  Node* CurrentContainer() const;
  int StartOffsetInCurrentContainer() const;
  int EndOffsetInCurrentContainer() const;
  PositionTemplate<Strategy> StartPositionInCurrentContainer() const;
  PositionTemplate<Strategy> EndPositionInCurrentContainer() const;

  const TextIteratorTextState& GetText() const { return *text_state_; }
  int length() const { return text_state_->length(); }
  UChar CharacterAt(unsigned index) const {
    return text_state_->CharacterAt(index);
  }

  bool BreaksAtReplacedElement() {
    return !behavior_.DoesNotBreakAtReplacedElement();
  }

  // Calculate the minimum |actualLength >= minLength| such that code units
  // with offset range [position, position + actualLength) are whole code
  // points. Append these code points to |output| and return |actualLength|.
  // TODO(xiaochengh): Use (start, end) instead of (start, length).
  int CopyTextTo(ForwardsTextBuffer* output,
                 int position,
                 int min_length) const;
  // TODO(xiaochengh): Avoid default parameters.
  int CopyTextTo(ForwardsTextBuffer* output, int position = 0) const;

  // Computes the length of the given range using a text iterator according to
  // the specified iteration behavior. The default iteration behavior is to
  // always emit object replacement characters for replaced elements.
  // TODO(editing-dev): We should remove start/end version of |RangeLength()|.
  static int RangeLength(
      const PositionTemplate<Strategy>& start,
      const PositionTemplate<Strategy>& end,
      const TextIteratorBehavior& =
          TextIteratorBehavior::DefaultRangeLengthBehavior());

  static int RangeLength(
      const EphemeralRangeTemplate<Strategy>&,
      const TextIteratorBehavior& =
          TextIteratorBehavior::DefaultRangeLengthBehavior());

  static bool ShouldEmitTabBeforeNode(Node*);
  static bool ShouldEmitNewlineBeforeNode(Node&);
  static bool ShouldEmitNewlineAfterNode(Node&);
  static bool ShouldEmitNewlineForNode(Node*, bool emits_original_text);

  static bool SupportsAltText(Node*);

 private:
  enum IterationProgress {
    kHandledNone,
    kHandledOpenShadowRoots,
    kHandledUserAgentShadowRoot,
    kHandledNode,
    kHandledChildren
  };

  void ExitNode();
  bool ShouldRepresentNodeOffsetZero();
  bool ShouldEmitSpaceBeforeAndAfterNode(Node*);
  void RepresentNodeOffsetZero();

  // Returns true if text is emitted from the remembered progress (if any).
  bool HandleRememberedProgress();

  void HandleTextNode();
  void HandleReplacedElement();
  void HandleNonTextNode();
  void SpliceBuffer(UChar,
                    Node* text_node,
                    Node* offset_base_node,
                    int text_start_offset,
                    int text_end_offset);

  // Used by selection preservation code. There should be one character emitted
  // between every VisiblePosition in the Range used to create the TextIterator.
  // FIXME <rdar://problem/6028818>: This functionality should eventually be
  // phased out when we rewrite moveParagraphs to not clone/destroy moved
  // content.
  bool EmitsCharactersBetweenAllVisiblePositions() const {
    return behavior_.EmitsCharactersBetweenAllVisiblePositions();
  }

  bool EntersTextControls() const { return behavior_.EntersTextControls(); }

  // Used in pasting inside password field.
  bool EmitsOriginalText() const { return behavior_.EmitsOriginalText(); }

  // Used when the visibility of the style should not affect text gathering.
  bool IgnoresStyleVisibility() const {
    return behavior_.IgnoresStyleVisibility();
  }

  // Used when the iteration should stop if form controls are reached.
  bool StopsOnFormControls() const { return behavior_.StopsOnFormControls(); }

  bool EmitsImageAltText() const { return behavior_.EmitsImageAltText(); }

  bool EntersOpenShadowRoots() const {
    return behavior_.EntersOpenShadowRoots();
  }

  bool EmitsObjectReplacementCharacter() const {
    return behavior_.EmitsObjectReplacementCharacter();
  }

  bool ExcludesAutofilledValue() const {
    return behavior_.ExcludeAutofilledValue();
  }

  bool DoesNotBreakAtReplacedElement() const {
    return behavior_.DoesNotBreakAtReplacedElement();
  }

  bool ForInnerText() const { return behavior_.ForInnerText(); }

  bool IsBetweenSurrogatePair(int position) const;

  // Append code units with offset range [position, position + copyLength)
  // to the output buffer.
  void CopyCodeUnitsTo(ForwardsTextBuffer* output,
                       int position,
                       int copy_length) const;

  // The range.
  const Member<Node> start_container_;
  const int start_offset_;
  const Member<Node> end_container_;
  const int end_offset_;
  // |m_endNode| stores |Strategy::childAt(*m_endContainer, m_endOffset - 1)|,
  // if it exists, or |nullptr| otherwise.
  const Member<Node> end_node_;
  const Member<Node> past_end_node_;

  // Current position, not necessarily of the text being returned, but position
  // as we walk through the DOM tree.
  Member<Node> node_;
  IterationProgress iteration_progress_;
  FullyClippedStateStackAlgorithm<Strategy> fully_clipped_stack_;
  int shadow_depth_;

  // Used when there is still some pending text from the current node; when
  // these are false, we go back to normal iterating.
  bool needs_another_newline_ = false;
  bool needs_handle_replaced_element_ = false;

  Member<Text> last_text_node_;

  const TextIteratorBehavior behavior_;

  // Used when stopsOnFormControls() is true to determine if the iterator should
  // keep advancing.
  bool should_stop_ = false;
  // Used for use counter |InnerTextWithShadowTree| and
  // |SelectionToStringWithShadowTree|, we should not use other purpose.
  bool handle_shadow_root_ = false;

  // Contains state of emitted text.
  const Member<TextIteratorTextState> text_state_;

  // Helper for extracting text content from text nodes.
  const Member<TextIteratorTextNodeHandler> text_node_handler_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    TextIteratorAlgorithm<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    TextIteratorAlgorithm<EditingInFlatTreeStrategy>;

using TextIterator = TextIteratorAlgorithm<EditingStrategy>;
using TextIteratorInFlatTree = TextIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink

#endif  // TextIterator_h
