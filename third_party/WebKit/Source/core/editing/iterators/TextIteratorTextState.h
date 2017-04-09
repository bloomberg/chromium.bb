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

#ifndef TextIteratorTextState_h
#define TextIteratorTextState_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/dom/Range.h"
#include "core/editing/iterators/ForwardsTextBuffer.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LayoutText;
class TextIteratorBehavior;

class CORE_EXPORT TextIteratorTextState {
  STACK_ALLOCATED();

 public:
  explicit TextIteratorTextState(const TextIteratorBehavior&);
  ~TextIteratorTextState() {}

  const String& GetString() const { return text_; }

  int length() const { return text_length_; }
  UChar CharacterAt(unsigned index) const;
  String Substring(unsigned position, unsigned length) const;
  void AppendTextToStringBuilder(StringBuilder&,
                                 unsigned position = 0,
                                 unsigned max_length = UINT_MAX) const;

  void SpliceBuffer(UChar,
                    Node* text_node,
                    Node* offset_base_node,
                    int text_start_offset,
                    int text_end_offset);
  void EmitText(Node* text_node,
                LayoutText* layout_object,
                int text_start_offset,
                int text_end_offset);
  void EmitAltText(Node*);
  void UpdateForReplacedElement(Node* base_node);
  void FlushPositionOffsets() const;
  int PositionStartOffset() const { return position_start_offset_; }
  int PositionEndOffset() const { return position_end_offset_; }
  Node* PositionNode() const { return position_node_; }
  bool HasEmitted() const { return has_emitted_; }
  UChar LastCharacter() const { return last_character_; }
  int TextStartOffset() const { return text_start_offset_; }
  void ResetRunInformation() {
    position_node_ = nullptr;
    text_length_ = 0;
  }

  void AppendTextTo(ForwardsTextBuffer* output,
                    unsigned position,
                    unsigned length_to_append) const;

 private:
  int text_length_;
  String text_;

  // Used for whitespace characters that aren't in the DOM, so we can point at
  // them.
  // If non-zero, overrides m_text.
  UChar single_character_buffer_;

  // The current text and its position, in the form to be returned from the
  // iterator.
  Member<Node> position_node_;
  mutable Member<Node> position_offset_base_node_;
  mutable int position_start_offset_;
  mutable int position_end_offset_;

  // Used when deciding whether to emit a "positioning" (e.g. newline) before
  // any other content
  bool has_emitted_;
  UChar last_character_;

  const TextIteratorBehavior behavior_;

  // Stores the length of :first-letter when we are at the remaining text.
  // Equals to 0 in all other cases.
  int text_start_offset_;

  DISALLOW_COPY_AND_ASSIGN(TextIteratorTextState);
};

}  // namespace blink

#endif  // TextIteratorTextState_h
