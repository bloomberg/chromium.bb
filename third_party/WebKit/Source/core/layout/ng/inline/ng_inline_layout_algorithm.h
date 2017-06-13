// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class NGConstraintSpace;
class NGInlineBreakToken;
class NGInlineNode;
class NGInlineItem;
class NGLineBoxFragmentBuilder;
class NGTextFragmentBuilder;

// A class for inline layout (e.g. a <span> with no special style).
//
// This class determines the position of NGInlineItem and build line boxes.
//
// Uses NGLineBreaker to find NGInlineItems to form a line.
class CORE_EXPORT NGInlineLayoutAlgorithm final
    : public NGLayoutAlgorithm<NGInlineNode, NGInlineBreakToken> {
 public:
  NGInlineLayoutAlgorithm(NGInlineNode,
                          NGConstraintSpace*,
                          NGInlineBreakToken* = nullptr);

  // The available width for the current line.
  LayoutUnit AvailableWidth() const;

  // Create a line.
  // @return false if the line does not fit in the constraint space in block
  //         direction.
  bool CreateLine(NGLineInfo*, RefPtr<NGInlineBreakToken> = nullptr);

  RefPtr<NGLayoutResult> Layout() override;

  // Lays out the inline float.
  // List of actions:
  // - tries to position the float right away if we have enough space.
  // - updates the current_opportunity if we actually place the float.
  // - if it's too wide then we add the float to the unpositioned list so it can
  //   be positioned after we're done with the current line.
  void LayoutAndPositionFloat(LayoutUnit end_position, LayoutObject*);

 private:
  bool IsHorizontalWritingMode() const { return is_horizontal_writing_mode_; }

  LayoutUnit LogicalLeftOffset() const;

  void BidiReorder(NGInlineItemResults*);

  bool PlaceItems(NGLineInfo*, RefPtr<NGInlineBreakToken>);
  NGInlineBoxState* PlaceAtomicInline(const NGInlineItem&,
                                      NGInlineItemResult*,
                                      bool is_first_line,
                                      LayoutUnit position,
                                      NGLineBoxFragmentBuilder*,
                                      NGTextFragmentBuilder*);

  void ApplyTextAlign(const ComputedStyle&,
                      ETextAlign,
                      LayoutUnit* line_left,
                      LayoutUnit inline_size,
                      LayoutUnit available_width);

  // Finds the next layout opportunity for the next text fragment.
  void FindNextLayoutOpportunity();

  NGInlineLayoutStateStack box_states_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  FontBaseline baseline_type_ = FontBaseline::kAlphabeticBaseline;

  NGLogicalOffset bfc_offset_;
  NGLogicalRect current_opportunity_;

  unsigned is_horizontal_writing_mode_ : 1;

  NGConstraintSpaceBuilder space_builder_;
  NGBoxStrut border_and_padding_;
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
