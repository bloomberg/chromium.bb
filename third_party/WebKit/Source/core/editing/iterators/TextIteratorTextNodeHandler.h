// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextIteratorTextNodeHandler_h
#define TextIteratorTextNodeHandler_h

#include "core/dom/Text.h"
#include "core/editing/iterators/TextIteratorBehavior.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class InlineTextBox;
class LayoutText;
class LayoutTextFragment;
class TextIteratorTextState;

// TextIteratorTextNodeHandler extracts plain text from a text node by calling
// HandleTextNode() function. It should be used only by TextIterator.
class TextIteratorTextNodeHandler {
  STACK_ALLOCATED();

 public:
  TextIteratorTextNodeHandler(const TextIteratorBehavior&,
                              TextIteratorTextState*);

  Text* GetNode() const { return text_node_; }

  // Returns true if more text is emitted without traversing to the next node.
  bool HandleRemainingTextRuns();

  // Returns true if a leading white space is emitted before a replaced element.
  bool FixLeadingWhiteSpaceForReplacedElement(Node*);

  void ResetCollapsedWhiteSpaceFixup();

  // Emit plain text from the given text node.
  void HandleTextNodeWhole(Text*);

  // Variants that emit plain text within the given DOM offset range.
  void HandleTextNodeStartFrom(Text*, int start_offset);
  void HandleTextNodeEndAt(Text*, int end_offset);
  void HandleTextNodeInRange(Text*, int start_offset, int end_offset);

 private:
  void HandlePreFormattedTextNode();
  void HandleTextBox();
  void HandleTextNodeFirstLetter(LayoutTextFragment*);
  bool ShouldHandleFirstLetter(const LayoutText&) const;
  bool ShouldProceedToRemainingText() const;
  void ProceedToRemainingText();
  size_t RestoreCollapsedTrailingSpace(InlineTextBox* next_text_box,
                                       size_t subrun_end);

  // Used when the visibility of the style should not affect text gathering.
  bool IgnoresStyleVisibility() const {
    return behavior_.IgnoresStyleVisibility();
  }

  void SpliceBuffer(UChar,
                    Node* text_node,
                    Node* offset_base_node,
                    int text_start_offset,
                    int text_end_offset);
  void EmitText(Node* text_node,
                LayoutText* layout_object,
                int text_start_offset,
                int text_end_offset);

  // The current text node and offset range, from which text should be emitted.
  Member<Text> text_node_;
  int offset_ = 0;
  int end_offset_ = 0;

  InlineTextBox* text_box_ = nullptr;

  // Remember if we are in the middle of handling a pre-formatted text node.
  bool needs_handle_pre_formatted_text_node_ = false;
  // Used when deciding text fragment created by :first-letter should be looked
  // into.
  bool handled_first_letter_ = false;
  // Used when iteration over :first-letter text to save pointer to
  // remaining text box.
  InlineTextBox* remaining_text_box_ = nullptr;
  // Used to point to LayoutText object for :first-letter.
  LayoutText* first_letter_text_ = nullptr;

  // Used to do the whitespace collapsing logic.
  bool last_text_node_ended_with_collapsed_space_ = false;

  // Used when text boxes are out of order (Hebrew/Arabic w/ embeded LTR text)
  Vector<InlineTextBox*> sorted_text_boxes_;
  size_t sorted_text_boxes_position_ = 0;

  const TextIteratorBehavior behavior_;

  // Contains state of emitted text.
  TextIteratorTextState& text_state_;

  DISALLOW_COPY_AND_ASSIGN(TextIteratorTextNodeHandler);
};

}  // namespace blink

#endif  // TextIteratorTextNodeHandler_h
