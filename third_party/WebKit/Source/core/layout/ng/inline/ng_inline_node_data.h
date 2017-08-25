// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineNodeData_h
#define NGInlineNodeData_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_item.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutBox;
class NGOffsetMappingResult;

// Data which is required for inline nodes.
struct CORE_EXPORT NGInlineNodeData {
  // The constructor and destructor can't be implicit or inlined, because they
  // need full definition of NGOffsetMappingResult.
  NGInlineNodeData();
  ~NGInlineNodeData();

 private:
  TextDirection BaseDirection() const {
    return static_cast<TextDirection>(base_direction_);
  }
  void SetBaseDirection(TextDirection direction) {
    base_direction_ = static_cast<unsigned>(direction);
  }

  friend class NGInlineNode;
  friend class NGInlineNodeForTest;
  friend class NGInlineNodeOffsetMappingTest;

  // Text content for all inline items represented by a single NGInlineNode.
  // Encoded either as UTF-16 or latin-1 depending on the content.
  String text_content_;
  Vector<NGInlineItem> items_;

  // |items_| to use for the first line, when the node has :first-line rules.
  // Items have different ComputedStyle, and may also have different ShapeResult
  // if fonts are different.
  std::unique_ptr<Vector<NGInlineItem>> first_line_items_;

  // The DOM to text content offset mapping of this inline node.
  std::unique_ptr<NGOffsetMappingResult> offset_mapping_;

  // next_sibling_ is only valid after NGInlineNode::PrepareLayout is called.
  // Calling NGInlineNode::NextSibling will trigger this.
  LayoutBox* next_sibling_;

  unsigned is_bidi_enabled_ : 1;
  unsigned base_direction_ : 1;  // TextDirection

  // We use this flag to determine if the inline node is empty, and will
  // produce a single zero block-size line box. If the node has text, atomic
  // inlines, open/close tags with margins/border/padding this will be false.
  unsigned is_empty_inline_ : 1;
};

}  // namespace blink

#endif  // NGInlineNode_h
