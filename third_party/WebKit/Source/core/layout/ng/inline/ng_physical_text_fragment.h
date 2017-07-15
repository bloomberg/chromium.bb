// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalTextFragment_h
#define NGPhysicalTextFragment_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

class ShapeResult;

// In CSS Writing Modes Levle 4, line orientation for layout and line
// orientation for paint are not always the same.
//
// Specifically, 'sideways-lr' typesets as if lines are horizontal flow, but
// rotates counterclockwise.
enum class NGLineOrientation {
  // Lines are horizontal.
  kHorizontal,
  // Lines are vertical, rotated clockwise. Inside of the line, it may be
  // typeset using vertical characteristics, horizontal characteristics, or
  // mixed. Lines flow left to right, or right to left.
  kClockWiseVertical,
  // Lines are vertical, rotated counterclockwise. Inside of the line is typeset
  // as if horizontal flow. Lines flow left to right.
  kCounterClockWiseVertical

  // When adding new values, ensure NGPhysicalTextFragment has enough bits.
};

class CORE_EXPORT NGPhysicalTextFragment final : public NGPhysicalFragment {
 public:
  NGPhysicalTextFragment(LayoutObject* layout_object,
                         const NGInlineNode node,
                         unsigned item_index,
                         unsigned start_offset,
                         unsigned end_offset,
                         NGPhysicalSize size,
                         NGLineOrientation line_orientation,
                         RefPtr<const ShapeResult> shape_result)
      : NGPhysicalFragment(layout_object, size, kFragmentText),
        node_(node),
        item_index_(item_index),
        start_offset_(start_offset),
        end_offset_(end_offset),
        shape_result_(shape_result),
        line_orientation_(static_cast<unsigned>(line_orientation)) {}

  const NGInlineNode Node() const { return node_; }
  StringView Text() const { return node_.Text(start_offset_, end_offset_); }

  const ShapeResult* TextShapeResult() const { return shape_result_.Get(); }

  // The range of NGLayoutInlineItem.
  unsigned ItemIndex() const { return item_index_; }
  unsigned StartOffset() const { return start_offset_; }
  unsigned EndOffset() const { return end_offset_; }

  NGLineOrientation LineOrientation() const {
    return static_cast<NGLineOrientation>(line_orientation_);
  }
  bool IsHorizontal() const {
    return LineOrientation() == NGLineOrientation::kHorizontal;
  }

 private:
  // TODO(kojii): NGInlineNode is to access text content and NGLayoutInlineItem.
  // Review if it's better to point them.
  const NGInlineNode node_;
  unsigned item_index_;
  unsigned start_offset_;
  unsigned end_offset_;

  RefPtr<const ShapeResult> shape_result_;

  unsigned line_orientation_ : 2;  // NGLineOrientation
};

DEFINE_TYPE_CASTS(NGPhysicalTextFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->IsText(),
                  fragment.IsText());

}  // namespace blink

#endif  // NGPhysicalTextFragment_h
