/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "public/web/WebSurroundingText.h"

#include "core/dom/Element.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/iterators/BackwardsCharacterIterator.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/forms/HTMLFormControlElement.h"

namespace blink {

namespace {

EphemeralRange ComputeRangeFromFrameSelection(WebLocalFrame* frame) {
  LocalFrame* web_frame = ToWebLocalFrameImpl(frame)->GetFrame();

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  web_frame->GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  return web_frame->Selection()
      .ComputeVisibleSelectionInDOMTree()
      .ToNormalizedEphemeralRange();
}

}  // namespace

WebSurroundingText::WebSurroundingText(WebLocalFrame* frame, size_t max_length)
    : WebSurroundingText(ComputeRangeFromFrameSelection(frame), max_length) {}

WebSurroundingText::WebSurroundingText(const EphemeralRange& range,
                                       size_t max_length)
    : start_offset_in_text_content_(0), end_offset_in_text_content_(0) {
  const Position start_position = range.StartPosition();
  const Position end_position = range.EndPosition();
  const unsigned half_max_length = max_length / 2;

  Document* document = start_position.GetDocument();

  // The position will have no document if it is null (as in no position).
  if (!document || !document->documentElement())
    return;
  DCHECK(!document->NeedsLayoutTreeUpdate());

  // The forward range starts at the selection end and ends at the document's
  // or the input element's end. It will then be updated to only contain the
  // text in the right range around the selection.
  DCHECK_EQ(RootEditableElementOf(start_position),
            RootEditableElementOf(end_position));
  Element* const root_editable = RootEditableElementOf(start_position);
  Element* const root_element =
      root_editable ? root_editable : document->documentElement();

  // Do not create surrounding text if start or end position is within a
  // control.
  if (HTMLFormControlElement::EnclosingFormControlElement(
          start_position.ComputeContainerNode()) ||
      HTMLFormControlElement::EnclosingFormControlElement(
          end_position.ComputeContainerNode()))
    return;

  CharacterIterator forward_iterator(
      end_position,
      Position::LastPositionInNode(*root_element).ParentAnchoredEquivalent(),
      TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());
  if (!forward_iterator.AtEnd())
    forward_iterator.Advance(max_length - half_max_length);

  // Same as with the forward range but with the backward range. The range
  // starts at the document's or input element's start and ends at the selection
  // start and will be updated.
  BackwardsCharacterIterator backwards_iterator(
      EphemeralRange(Position::FirstPositionInNode(*root_element)
                         .ParentAnchoredEquivalent(),
                     start_position),
      TextIteratorBehavior::Builder().SetStopsOnFormControls(true).Build());
  if (!backwards_iterator.AtEnd())
    backwards_iterator.Advance(half_max_length);

  // Upon some conditions backwards iterator yields invalid EndPosition() which
  // causes a crash in TextIterator::RangeLength().
  // TODO(editing-dev): Fix BackwardsCharacterIterator, http://crbug.com/758438.
  if (backwards_iterator.EndPosition() > start_position ||
      end_position > forward_iterator.StartPosition())
    return;

  const TextIteratorBehavior behavior =
      TextIteratorBehavior::NoTrailingSpaceRangeLengthBehavior();
  const Position content_start = backwards_iterator.EndPosition();
  const Position content_end = forward_iterator.StartPosition();
  start_offset_in_text_content_ =
      TextIterator::RangeLength(content_start, start_position, behavior);
  end_offset_in_text_content_ =
      TextIterator::RangeLength(content_start, end_position, behavior);
  text_content_ = PlainText(
      EphemeralRange(content_start, content_end),
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
}

WebString WebSurroundingText::TextContent() const {
  return text_content_;
}

size_t WebSurroundingText::StartOffsetInTextContent() const {
  return start_offset_in_text_content_;
}

size_t WebSurroundingText::EndOffsetInTextContent() const {
  return end_offset_in_text_content_;
}

bool WebSurroundingText::IsEmpty() const {
  return text_content_.IsEmpty();
}

}  // namespace blink
