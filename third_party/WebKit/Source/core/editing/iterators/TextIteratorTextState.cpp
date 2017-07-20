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

#include "core/editing/iterators/TextIteratorTextState.h"

#include "core/html/HTMLElement.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

UChar TextIteratorTextState::CharacterAt(unsigned index) const {
  SECURITY_DCHECK(index < static_cast<unsigned>(length()));
  if (!(index < static_cast<unsigned>(length())))
    return 0;

  if (single_character_buffer_) {
    DCHECK_EQ(index, 0u);
    DCHECK_EQ(length(), 1);
    return single_character_buffer_;
  }

  return text_[text_start_offset_ + index];
}

String TextIteratorTextState::Substring(unsigned position,
                                        unsigned length) const {
  SECURITY_DCHECK(position <= static_cast<unsigned>(this->length()));
  SECURITY_DCHECK(position + length <= static_cast<unsigned>(this->length()));
  if (!length)
    return g_empty_string;
  if (single_character_buffer_) {
    DCHECK_EQ(position, 0u);
    DCHECK_EQ(length, 1u);
    return String(&single_character_buffer_, 1);
  }
  return text_.Substring(text_start_offset_ + position, length);
}

void TextIteratorTextState::AppendTextToStringBuilder(
    StringBuilder& builder,
    unsigned position,
    unsigned max_length) const {
  unsigned length_to_append =
      std::min(static_cast<unsigned>(length()) - position, max_length);
  if (!length_to_append)
    return;
  if (single_character_buffer_) {
    DCHECK_EQ(position, 0u);
    builder.Append(single_character_buffer_);
  } else {
    builder.Append(text_, text_start_offset_ + position, length_to_append);
  }
}

void TextIteratorTextState::UpdateForReplacedElement(Node* base_node) {
  has_emitted_ = true;
  position_node_ = base_node->parentNode();
  position_offset_base_node_ = base_node;
  position_start_offset_ = 0;
  position_end_offset_ = 1;
  single_character_buffer_ = 0;

  text_length_ = 0;
  text_start_offset_ = 0;
  last_character_ = 0;
}

void TextIteratorTextState::EmitAltText(Node* node) {
  text_ = ToHTMLElement(node)->AltText();
  text_start_offset_ = 0;
  text_length_ = text_.length();
  last_character_ = text_length_ ? text_[text_length_ - 1] : 0;
}

void TextIteratorTextState::FlushPositionOffsets() const {
  if (!position_offset_base_node_)
    return;
  int index = position_offset_base_node_->NodeIndex();
  position_start_offset_ += index;
  position_end_offset_ += index;
  position_offset_base_node_ = nullptr;
}

void TextIteratorTextState::SpliceBuffer(UChar c,
                                         Node* text_node,
                                         Node* offset_base_node,
                                         int text_start_offset,
                                         int text_end_offset) {
  DCHECK(text_node);
  has_emitted_ = true;

  // Remember information with which to construct the TextIterator::range().
  // NOTE: textNode is often not a text node, so the range will specify child
  // nodes of positionNode
  position_node_ = text_node;
  position_offset_base_node_ = offset_base_node;
  position_start_offset_ = text_start_offset;
  position_end_offset_ = text_end_offset;

  // remember information with which to construct the TextIterator::characters()
  // and length()
  single_character_buffer_ = c;
  DCHECK(single_character_buffer_);
  text_length_ = 1;
  text_start_offset_ = 0;

  // remember some iteration state
  last_character_ = c;
}

void TextIteratorTextState::EmitText(Node* text_node,
                                     int position_start_offset,
                                     int position_end_offset,
                                     const String& string,
                                     int text_start_offset,
                                     int text_end_offset) {
  DCHECK(text_node);
  text_ = string;

  DCHECK(!text_.IsEmpty());
  DCHECK_LE(0, text_start_offset);
  DCHECK_LT(text_start_offset, static_cast<int>(text_.length()));
  DCHECK_LE(0, text_end_offset);
  DCHECK_LE(text_end_offset, static_cast<int>(text_.length()));
  DCHECK_LE(text_start_offset, text_end_offset);

  position_node_ = text_node;
  position_offset_base_node_ = nullptr;
  position_start_offset_ = position_start_offset;
  position_end_offset_ = position_end_offset;
  single_character_buffer_ = 0;
  text_start_offset_ = text_start_offset;
  text_length_ = text_end_offset - text_start_offset;
  last_character_ = text_[text_end_offset - 1];

  has_emitted_ = true;
}

void TextIteratorTextState::AppendTextTo(ForwardsTextBuffer* output,
                                         unsigned position,
                                         unsigned length_to_append) const {
  SECURITY_DCHECK(position + length_to_append <=
                  static_cast<unsigned>(length()));
  // Make sure there's no integer overflow.
  SECURITY_DCHECK(position + length_to_append >= position);
  if (!length_to_append)
    return;
  DCHECK(output);
  if (single_character_buffer_) {
    DCHECK_EQ(position, 0u);
    DCHECK_EQ(length(), 1);
    output->PushCharacters(single_character_buffer_, 1);
    return;
  }
  unsigned offset = text_start_offset_ + position;
  if (text_.Is8Bit())
    output->PushRange(text_.Characters8() + offset, length_to_append);
  else
    output->PushRange(text_.Characters16() + offset, length_to_append);
}

}  // namespace blink
