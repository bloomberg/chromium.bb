/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/SurroundingText.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/Position.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/CharacterIterator.h"

namespace blink {

SurroundingText::SurroundingText(const Range& range, unsigned max_length)
    : start_offset_in_content_(0), end_offset_in_content_(0) {
  Initialize(range.StartPosition(), range.EndPosition(), max_length);
}

SurroundingText::SurroundingText(const Position& position, unsigned max_length)
    : start_offset_in_content_(0), end_offset_in_content_(0) {
  Initialize(position, position, max_length);
}

void SurroundingText::Initialize(const Position& start_position,
                                 const Position& end_position,
                                 unsigned max_length) {
  DCHECK_EQ(start_position.GetDocument(), end_position.GetDocument());

  const unsigned half_max_length = max_length / 2;

  Document* document = start_position.GetDocument();
  // The position will have no document if it is null (as in no position).
  if (!document || !document->documentElement())
    return;
  DCHECK(!document->NeedsLayoutTreeUpdate());

  // The forward range starts at the selection end and ends at the document's
  // end. It will then be updated to only contain the text in the text in the
  // right range around the selection.
  CharacterIterator forward_iterator(
      end_position,
      Position::LastPositionInNode(document->documentElement())
          .ParentAnchoredEquivalent(),
      TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());
  // FIXME: why do we stop going trough the text if we were not able to select
  // something on the right?
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(max_length - half_max_length);

  EphemeralRange forward_range = forward_iterator.Range();
  if (forward_range.IsNull() ||
      !Range::Create(*document, end_position, forward_range.StartPosition())
           ->GetText()
           .length())
    return;

  // Same as with the forward range but with the backward range. The range
  // starts at the document's start and ends at the selection start and will
  // be updated.
  BackwardsCharacterIterator backwards_iterator(
      Position::FirstPositionInNode(document->documentElement())
          .ParentAnchoredEquivalent(),
      start_position,
      TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());
  if (!backwards_iterator.AtEnd())
    backwards_iterator.Advance(half_max_length);

  const TextIteratorBehavior behavior =
      TextIteratorBehavior::NoTrailingSpaceRangeLengthBehavior();
  start_offset_in_content_ = TextIterator::RangeLength(
      backwards_iterator.EndPosition(), start_position, behavior);
  end_offset_in_content_ = TextIterator::RangeLength(
      backwards_iterator.EndPosition(), end_position, behavior);
  content_range_ = Range::Create(*document, backwards_iterator.EndPosition(),
                                 forward_range.StartPosition());
  DCHECK(content_range_);
}

String SurroundingText::Content() const {
  if (content_range_) {
    // SurroundingText is created with clean layout and must not be stored
    // through DOM or style changes, so layout must still be clean here.
    DCHECK(!content_range_->OwnerDocument().NeedsLayoutTreeUpdate());
    return content_range_->GetText();
  }
  return String();
}

unsigned SurroundingText::StartOffsetInContent() const {
  return start_offset_in_content_;
}

unsigned SurroundingText::EndOffsetInContent() const {
  return end_offset_in_content_;
}

}  // namespace blink
