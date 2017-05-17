// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItemResult_h
#define NGInlineItemResult_h

#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/ng_layout_result.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// The result of measuring NGInlineItem.
//
// This is a transient context object only while building line boxes.
// Produced while determining the line break points, but these data are needed
// to create line boxes.
//
// NGLineBreaker produces, and NGInlineLayoutAlgorithm consumes.
struct CORE_EXPORT NGInlineItemResult {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // The index of NGInlineItem and its text range.
  unsigned item_index;
  unsigned start_offset;
  unsigned end_offset;

  // Inline size of this item.
  LayoutUnit inline_size;

  // ShapeResult for text items. Maybe different from NGInlineItem if re-shape
  // is needed in the line breaker.
  RefPtr<ShapeResult> shape_result;

  // NGLayoutResult for atomic inline items.
  RefPtr<NGLayoutResult> layout_result;

  // NGBoxStrut for atomic inline items.
  NGBoxStrut margins;

  NGInlineItemResult();
  NGInlineItemResult(unsigned index, unsigned start, unsigned end);
};

// Represents a set of NGInlineItemResult that form a line box.
using NGInlineItemResults = Vector<NGInlineItemResult, 32>;

}  // namespace blink

#endif  // NGInlineItemResult_h
