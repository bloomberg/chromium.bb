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

#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"

#include "core/dom/FirstLetterPseudoElement.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/html/forms/HTMLFormControlElement.h"
#include "core/layout/LayoutTextFragment.h"

namespace blink {

static int CollapsedSpaceLength(LayoutText* layout_text, int text_end) {
  const String& text = layout_text->GetText();
  int length = text.length();
  for (int i = text_end; i < length; ++i) {
    if (!layout_text->Style()->IsCollapsibleWhiteSpace(text[i]))
      return i - text_end;
  }

  return length - text_end;
}

static int MaxOffsetIncludingCollapsedSpaces(Node* node) {
  int offset = CaretMaxOffset(node);

  if (node->GetLayoutObject() && node->GetLayoutObject()->IsText()) {
    offset +=
        CollapsedSpaceLength(ToLayoutText(node->GetLayoutObject()), offset) +
        ToLayoutText(node->GetLayoutObject())->TextStartOffset();
  }

  return offset;
}

template <typename Strategy>
SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::
    SimplifiedBackwardsTextIteratorAlgorithm(
        const EphemeralRangeTemplate<Strategy>& range,
        const TextIteratorBehavior& behavior)
    : node_(nullptr),
      offset_(0),
      handled_node_(false),
      handled_children_(false),
      start_node_(nullptr),
      start_offset_(0),
      end_node_(nullptr),
      end_offset_(0),
      position_node_(nullptr),
      position_start_offset_(0),
      position_end_offset_(0),
      text_offset_(0),
      text_length_(0),
      single_character_buffer_(0),
      have_passed_start_node_(false),
      should_handle_first_letter_(false),
      stops_on_form_controls_(behavior.StopsOnFormControls()),
      should_stop_(false),
      emits_original_text_(false) {
  DCHECK(
      behavior == TextIteratorBehavior() ||
      behavior ==
          TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());

  Node* start_node = range.StartPosition().AnchorNode();
  if (!start_node)
    return;
  Node* end_node = range.EndPosition().AnchorNode();
  int start_offset = range.StartPosition().ComputeEditingOffset();
  int end_offset = range.EndPosition().ComputeEditingOffset();

  Init(start_node, end_node, start_offset, end_offset);
}

template <typename Strategy>
void SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::Init(Node* start_node,
                                                              Node* end_node,
                                                              int start_offset,
                                                              int end_offset) {
  if (!start_node->IsCharacterDataNode() && start_offset >= 0) {
    // |Strategy::childAt()| will return 0 if the offset is out of range. We
    // rely on this behavior instead of calling |countChildren()| to avoid
    // traversing the children twice.
    if (Node* child_at_offset = Strategy::ChildAt(*start_node, start_offset)) {
      start_node = child_at_offset;
      start_offset = 0;
    }
  }
  if (!end_node->IsCharacterDataNode() && end_offset > 0) {
    // |Strategy::childAt()| will return 0 if the offset is out of range. We
    // rely on this behavior instead of calling |countChildren()| to avoid
    // traversing the children twice.
    if (Node* child_at_offset = Strategy::ChildAt(*end_node, end_offset - 1)) {
      end_node = child_at_offset;
      end_offset = Position::LastOffsetInNode(*end_node);
    }
  }

  node_ = end_node;
  fully_clipped_stack_.SetUpFullyClippedStack(node_);
  offset_ = end_offset;
  handled_node_ = false;
  handled_children_ = !end_offset;

  start_node_ = start_node;
  start_offset_ = start_offset;
  end_node_ = end_node;
  end_offset_ = end_offset;

#if DCHECK_IS_ON()
  // Need this just because of the assert.
  position_node_ = end_node;
#endif

  have_passed_start_node_ = false;

  Advance();
}

template <typename Strategy>
void SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::Advance() {
  DCHECK(position_node_);

  if (should_stop_)
    return;

  if (stops_on_form_controls_ &&
      HTMLFormControlElement::EnclosingFormControlElement(node_)) {
    should_stop_ = true;
    return;
  }

  position_node_ = nullptr;
  text_length_ = 0;

  while (node_ && !have_passed_start_node_) {
    // Don't handle node if we start iterating at [node, 0].
    if (!handled_node_ && !(node_ == end_node_ && !end_offset_)) {
      LayoutObject* layout_object = node_->GetLayoutObject();
      if (layout_object && layout_object->IsText() &&
          node_->getNodeType() == Node::kTextNode) {
        // FIXME: What about kCdataSectionNode?
        if (layout_object->Style()->Visibility() == EVisibility::kVisible &&
            offset_ > 0)
          handled_node_ = HandleTextNode();
      } else if (layout_object && (layout_object->IsLayoutEmbeddedContent() ||
                                   TextIterator::SupportsAltText(node_))) {
        if (layout_object->Style()->Visibility() == EVisibility::kVisible &&
            offset_ > 0)
          handled_node_ = HandleReplacedElement();
      } else {
        handled_node_ = HandleNonTextNode();
      }
      if (position_node_)
        return;
    }

    if (!handled_children_ && Strategy::HasChildren(*node_)) {
      node_ = Strategy::LastChild(*node_);
      fully_clipped_stack_.PushFullyClippedState(node_);
    } else {
      // Exit empty containers as we pass over them or containers
      // where [container, 0] is where we started iterating.
      if (!handled_node_ && CanHaveChildrenForEditing(node_) &&
          Strategy::Parent(*node_) &&
          (!Strategy::LastChild(*node_) ||
           (node_ == end_node_ && !end_offset_))) {
        ExitNode();
        if (position_node_) {
          handled_node_ = true;
          handled_children_ = true;
          return;
        }
      }

      // Exit all other containers.
      while (!Strategy::PreviousSibling(*node_)) {
        if (!AdvanceRespectingRange(
                ParentCrossingShadowBoundaries<Strategy>(*node_)))
          break;
        fully_clipped_stack_.Pop();
        ExitNode();
        if (position_node_) {
          handled_node_ = true;
          handled_children_ = true;
          return;
        }
      }

      fully_clipped_stack_.Pop();
      if (AdvanceRespectingRange(Strategy::PreviousSibling(*node_)))
        fully_clipped_stack_.PushFullyClippedState(node_);
      else
        node_ = nullptr;
    }

    // For the purpose of word boundary detection,
    // we should iterate all visible text and trailing (collapsed) whitespaces.
    offset_ = node_ ? MaxOffsetIncludingCollapsedSpaces(node_) : 0;
    handled_node_ = false;
    handled_children_ = false;

    if (position_node_)
      return;
  }
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::HandleTextNode() {
  int start_offset;
  int offset_in_node;
  LayoutText* layout_object = HandleFirstLetter(start_offset, offset_in_node);
  if (!layout_object)
    return true;

  String text = layout_object->GetText();
  if (!layout_object->HasTextBoxes() && text.length() > 0)
    return true;

  position_end_offset_ = offset_;
  offset_ = start_offset;
  position_node_ = node_;
  position_start_offset_ = offset_;

  DCHECK_LE(0, position_start_offset_ - offset_in_node);
  DCHECK_LE(position_start_offset_ - offset_in_node,
            static_cast<int>(text.length()));
  DCHECK_LE(1, position_end_offset_ - offset_in_node);
  DCHECK_LE(position_end_offset_ - offset_in_node,
            static_cast<int>(text.length()));
  DCHECK_LE(position_start_offset_, position_end_offset_);

  text_length_ = position_end_offset_ - position_start_offset_;
  text_offset_ = position_start_offset_ - offset_in_node;
  text_container_ = text;
  single_character_buffer_ = 0;
  CHECK_LE(static_cast<unsigned>(text_offset_ + text_length_), text.length());

  return !should_handle_first_letter_;
}

template <typename Strategy>
LayoutText* SimplifiedBackwardsTextIteratorAlgorithm<
    Strategy>::HandleFirstLetter(int& start_offset, int& offset_in_node) {
  LayoutText* layout_object = ToLayoutText(node_->GetLayoutObject());
  start_offset = (node_ == start_node_) ? start_offset_ : 0;

  if (!layout_object->IsTextFragment()) {
    offset_in_node = 0;
    return layout_object;
  }

  LayoutTextFragment* fragment = ToLayoutTextFragment(layout_object);
  int offset_after_first_letter = fragment->Start();
  if (start_offset >= offset_after_first_letter) {
    // We'll stop in remaining part.
    DCHECK(!should_handle_first_letter_);
    offset_in_node = offset_after_first_letter;
    return layout_object;
  }

  if (!should_handle_first_letter_ && offset_after_first_letter < offset_) {
    // Enter into remaining part
    should_handle_first_letter_ = true;
    offset_in_node = offset_after_first_letter;
    start_offset = offset_after_first_letter;
    return layout_object;
  }

  // Enter into first-letter part
  should_handle_first_letter_ = false;
  offset_in_node = 0;

  DCHECK(fragment->IsRemainingTextLayoutObject());
  DCHECK(fragment->GetFirstLetterPseudoElement());

  LayoutObject* pseudo_element_layout_object =
      fragment->GetFirstLetterPseudoElement()->GetLayoutObject();
  DCHECK(pseudo_element_layout_object);
  DCHECK(pseudo_element_layout_object->SlowFirstChild());
  LayoutText* first_letter_layout_object =
      ToLayoutText(pseudo_element_layout_object->SlowFirstChild());

  const int end_offset =
      end_node_ == node_ && end_offset_ < offset_after_first_letter
          ? end_offset_
          : first_letter_layout_object->CaretMaxOffset();
  offset_ =
      end_offset + CollapsedSpaceLength(first_letter_layout_object, end_offset);

  return first_letter_layout_object;
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<
    Strategy>::HandleReplacedElement() {
  unsigned index = Strategy::Index(*node_);
  // We want replaced elements to behave like punctuation for boundary
  // finding, and to simply take up space for the selection preservation
  // code in moveParagraphs, so we use a comma. Unconditionally emit
  // here because this iterator is only used for boundary finding.
  EmitCharacter(',', Strategy::Parent(*node_), index, index + 1);
  return true;
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::HandleNonTextNode() {
  // We can use a linefeed in place of a tab because this simple iterator is
  // only used to find boundaries, not actual content. A linefeed breaks words,
  // sentences, and paragraphs.
  if (TextIterator::ShouldEmitNewlineForNode(node_, emits_original_text_) ||
      TextIterator::ShouldEmitNewlineAfterNode(*node_) ||
      TextIterator::ShouldEmitTabBeforeNode(node_)) {
    unsigned index = Strategy::Index(*node_);
    // The start of this emitted range is wrong. Ensuring correctness would
    // require VisiblePositions and so would be slow. previousBoundary expects
    // this.
    EmitCharacter('\n', Strategy::Parent(*node_), index + 1, index + 1);
  }
  return true;
}

template <typename Strategy>
void SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::ExitNode() {
  if (TextIterator::ShouldEmitNewlineForNode(node_, emits_original_text_) ||
      TextIterator::ShouldEmitNewlineBeforeNode(*node_) ||
      TextIterator::ShouldEmitTabBeforeNode(node_)) {
    // The start of this emitted range is wrong. Ensuring correctness would
    // require VisiblePositions and so would be slow. previousBoundary expects
    // this.
    EmitCharacter('\n', node_, 0, 0);
  }
}

template <typename Strategy>
void SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::EmitCharacter(
    UChar c,
    Node* node,
    int start_offset,
    int end_offset) {
  single_character_buffer_ = c;
  position_node_ = node;
  position_start_offset_ = start_offset;
  position_end_offset_ = end_offset;
  text_offset_ = 0;
  text_length_ = 1;
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::AdvanceRespectingRange(
    Node* next) {
  if (!next)
    return false;
  have_passed_start_node_ |= node_ == start_node_;
  if (have_passed_start_node_)
    return false;
  node_ = next;
  return true;
}

template <typename Strategy>
Node* SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::StartContainer()
    const {
  if (position_node_)
    return position_node_;
  return start_node_;
}

template <typename Strategy>
int SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::EndOffset() const {
  if (position_node_)
    return position_end_offset_;
  return start_offset_;
}

template <typename Strategy>
PositionTemplate<Strategy>
SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::StartPosition() const {
  if (position_node_)
    return PositionTemplate<Strategy>::EditingPositionOf(
        position_node_, position_start_offset_);
  return PositionTemplate<Strategy>::EditingPositionOf(start_node_,
                                                       start_offset_);
}

template <typename Strategy>
PositionTemplate<Strategy>
SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::EndPosition() const {
  if (position_node_)
    return PositionTemplate<Strategy>::EditingPositionOf(position_node_,
                                                         position_end_offset_);
  return PositionTemplate<Strategy>::EditingPositionOf(start_node_,
                                                       start_offset_);
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::IsInTextSecurityMode()
    const {
  return IsTextSecurityNode(GetNode());
}

template <typename Strategy>
UChar SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::CharacterAt(
    unsigned index) const {
  // TODO(xiaochengh): Mostly copied from TextIteratorTextState::characterAt.
  // Should try to improve the code quality by reusing the code.
  SECURITY_DCHECK(index < static_cast<unsigned>(length()));
  if (!(index < static_cast<unsigned>(length())))
    return 0;
  if (single_character_buffer_) {
    DCHECK_EQ(index, 0u);
    DCHECK_EQ(length(), 1);
    return single_character_buffer_;
  }
  return text_container_[text_offset_ + text_length_ - 1 - index];
}

template <typename Strategy>
bool SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::IsBetweenSurrogatePair(
    int position) const {
  DCHECK_GE(position, 0);
  return position > 0 && position < length() &&
         U16_IS_TRAIL(CharacterAt(position - 1)) &&
         U16_IS_LEAD(CharacterAt(position));
}

template <typename Strategy>
int SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::CopyTextTo(
    BackwardsTextBuffer* output,
    int position,
    int min_length) const {
  int end = std::min(length(), position + min_length);
  if (IsBetweenSurrogatePair(end))
    ++end;
  int copied_length = end - position;
  CopyCodeUnitsTo(output, position, copied_length);
  return copied_length;
}

template <typename Strategy>
int SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::CopyTextTo(
    BackwardsTextBuffer* output,
    int position) const {
  return CopyTextTo(output, position, text_length_ - position);
}

template <typename Strategy>
void SimplifiedBackwardsTextIteratorAlgorithm<Strategy>::CopyCodeUnitsTo(
    BackwardsTextBuffer* output,
    int position,
    int copy_length) const {
  DCHECK_GE(position, 0);
  DCHECK_GE(copy_length, 0);
  DCHECK_LE(position + copy_length, text_length_);
  // Make sure there's no integer overflow.
  DCHECK_GE(position + copy_length, position);
  if (text_length_ == 0 || copy_length == 0)
    return;
  DCHECK(output);
  if (single_character_buffer_) {
    output->PushCharacters(single_character_buffer_, 1);
    return;
  }
  int offset = text_offset_ + text_length_ - position - copy_length;
  if (text_container_.Is8Bit())
    output->PushRange(text_container_.Characters8() + offset, copy_length);
  else
    output->PushRange(text_container_.Characters16() + offset, copy_length);
}

template class CORE_TEMPLATE_EXPORT
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
