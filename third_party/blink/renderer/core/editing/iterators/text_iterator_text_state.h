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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_STATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/iterators/forwards_text_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BackwardsTextBuffer;

class CORE_EXPORT TextIteratorTextState {
  STACK_ALLOCATED();

 public:
  explicit TextIteratorTextState(const TextIteratorBehavior&);

  // Return properties of the current text.
  unsigned length() const { return text_length_; }
  UChar CharacterAt(unsigned index) const;
  String Substring(unsigned position, unsigned length) const;
  void AppendTextToStringBuilder(StringBuilder&,
                                 unsigned position = 0,
                                 unsigned max_length = UINT_MAX) const;
  void AppendTextTo(ForwardsTextBuffer* output,
                    unsigned position,
                    unsigned length_to_append) const;
  void PrependTextTo(BackwardsTextBuffer* output,
                     unsigned position,
                     unsigned length_to_prepend) const;

  void SpliceBuffer(UChar,
                    const Node* text_node,
                    const Node* offset_base_node,
                    unsigned text_start_offset,
                    unsigned text_end_offset);
  void EmitText(const Node*,
                unsigned position_start_offset,
                unsigned position_end_offset,
                const String&,
                unsigned text_start_offset,
                unsigned text_end_offset);
  void EmitAltText(const Node*);
  void UpdateForReplacedElement(const Node* base_node);

  // Return position of the current text.
  void FlushPositionOffsets() const;
  unsigned PositionStartOffset() const { return position_start_offset_; }
  unsigned PositionEndOffset() const { return position_end_offset_; }
  const Node* PositionNode() const { return position_node_; }

  bool HasEmitted() const { return has_emitted_; }
  UChar LastCharacter() const { return last_character_; }
  void ResetRunInformation() {
    position_node_ = nullptr;
    text_length_ = 0;
  }

 private:
  const TextIteratorBehavior behavior_;
  unsigned text_length_ = 0;

  // Used for whitespace characters that aren't in the DOM, so we can point at
  // them.
  // If non-zero, overrides |text_|.
  UChar single_character_buffer_ = 0;

  // The current text when |single_character_buffer_| is zero, in which case it
  // is |text_.Substring(text_start_offset_, text_length_)|.
  String text_;
  unsigned text_start_offset_ = 0;

  // Position of the current text, in the form to be returned from the iterator.
  Member<const Node> position_node_;
  mutable Member<const Node> position_offset_base_node_;
  mutable unsigned position_start_offset_ = 0;
  mutable unsigned position_end_offset_ = 0;

  // Used when deciding whether to emit a "positioning" (e.g. newline) before
  // any other content
  bool has_emitted_ = false;
  UChar last_character_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TextIteratorTextState);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_ITERATOR_TEXT_STATE_H_
