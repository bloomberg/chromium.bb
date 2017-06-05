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

// Data which is required for inline nodes.
struct CORE_EXPORT NGInlineNodeData {
 private:
  friend class NGInlineNode;
  friend class NGInlineNodeForTest;

  // Text content for all inline items represented by a single NGInlineNode.
  // Encoded either as UTF-16 or latin-1 depending on the content.
  String text_content_;
  Vector<NGInlineItem> items_;

  // next_sibling_ is only valid after NGInlineNode::PrepareLayout is called.
  // Calling NGInlineNode::NextSibling will trigger this.
  LayoutBox* next_sibling_;

  // start_inline_ must always be reset within the constructor of NGInlineNode.
  LayoutObject* start_inline_;

  // TODO(kojii): This should move to somewhere else when we move PrepareLayout
  // to the correct place.
  bool is_bidi_enabled_ = false;
};

}  // namespace blink

#endif  // NGInlineNode_h
