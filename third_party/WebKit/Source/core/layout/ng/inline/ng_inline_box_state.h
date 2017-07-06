// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBoxState_h
#define NGInlineBoxState_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/wtf/Vector.h"

namespace blink {

class NGInlineItem;
struct NGInlineItemResult;
class NGLineBoxFragmentBuilder;

// Fragments that require the layout position/size of ancestor are packed in
// this struct.
struct NGPendingPositions {
  unsigned fragment_start;
  unsigned fragment_end;
  NGLineHeightMetrics metrics;
  EVerticalAlign vertical_align;
};

// Represents the current box while NGInlineLayoutAlgorithm performs layout.
// Used 1) to cache common values for a box, and 2) to layout children that
// require ancestor position or size.
// This is a transient object only while building line boxes in a block.
struct NGInlineBoxState {
  unsigned fragment_start;
  const NGInlineItem* item;
  const ComputedStyle* style;

  // The united metrics for the current box. This includes all objects in this
  // box, including descendants, and adjusted by placement properties such as
  // 'vertical-align'.
  NGLineHeightMetrics metrics;

  // The metrics of the font for this box. This includes leadings as specified
  // by the 'line-height' property.
  NGLineHeightMetrics text_metrics;

  // The distance between the text-top and the baseline for this box. The
  // text-top does not include leadings.
  LayoutUnit text_top;

  // These values are to create a box fragment. Set only when needs_box_fragment
  // is set.
  LayoutUnit line_left_position;
  LayoutUnit line_right_position;
  LayoutUnit borders_paddings_block_start;
  LayoutUnit borders_paddings_block_end;
  NGBorderEdges border_edges;

  Vector<NGPendingPositions> pending_descendants;
  bool include_used_fonts = false;
  bool needs_box_fragment = false;
  bool needs_box_fragment_when_empty = false;

  // Compute text metrics for a box. All text in a box share the same metrics.
  void ComputeTextMetrics(const ComputedStyle& style, FontBaseline);
  void AccumulateUsedFonts(const NGInlineItem&,
                           unsigned start,
                           unsigned end,
                           FontBaseline);

  // Create a box fragment for this box.
  void SetNeedsBoxFragment(bool when_empty);
  void SetLineRightForBoxFragment(const NGInlineItem&,
                                  const NGInlineItemResult&,
                                  LayoutUnit position);
};

// Represents the inline tree structure. This class provides:
// 1) Allow access to fragments belonging to the current box.
// 2) Performs layout when the positin/size of a box was computed.
// 3) Cache common values for a box.
class NGInlineLayoutStateStack {
 public:
  // The box state for the line box.
  NGInlineBoxState& LineBoxState() { return stack_.front(); }

  // Initialize the box state stack for a new line.
  // @return The initial box state for the line.
  NGInlineBoxState* OnBeginPlaceItems(const ComputedStyle*, FontBaseline);

  // Push a box state stack.
  NGInlineBoxState* OnOpenTag(const NGInlineItem&,
                              const NGInlineItemResult&,
                              NGLineBoxFragmentBuilder*,
                              LayoutUnit position);

  // Pop a box state stack.
  NGInlineBoxState* OnCloseTag(const NGInlineItem&,
                               NGLineBoxFragmentBuilder*,
                               NGInlineBoxState*,
                               FontBaseline);

  // Compute all the pending positioning at the end of a line.
  void OnEndPlaceItems(NGLineBoxFragmentBuilder*,
                       FontBaseline,
                       LayoutUnit position);

 private:
  // End of a box state, either explicitly by close tag, or implicitly at the
  // end of a line.
  void EndBoxState(NGInlineBoxState*, NGLineBoxFragmentBuilder*, FontBaseline);

  void AddBoxFragmentPlaceholder(NGInlineBoxState*,
                                 NGLineBoxFragmentBuilder*,
                                 FontBaseline);
  void CreateBoxFragments(NGLineBoxFragmentBuilder*);

  enum PositionPending { kPositionNotPending, kPositionPending };

  // Compute vertical position for the 'vertical-align' property.
  // The timing to apply varies by values; some values apply at the layout of
  // the box was computed. Other values apply when the layout of the parent or
  // the line box was computed.
  // https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
  // https://www.w3.org/TR/css-inline-3/#propdef-vertical-align
  PositionPending ApplyBaselineShift(NGInlineBoxState*,
                                     NGLineBoxFragmentBuilder*,
                                     FontBaseline);

  // Data for a box fragment placeholder. See AddBoxFragmentPlaceholder().
  // This is a transient object only while building a line box.
  struct BoxFragmentPlaceholder {
    unsigned fragment_start;
    unsigned fragment_end;
    const NGInlineItem* item;
    NGLogicalSize size;
    NGBorderEdges border_edges;
  };

  Vector<NGInlineBoxState, 4> stack_;
  Vector<BoxFragmentPlaceholder, 4> box_placeholders_;
};

}  // namespace blink

#endif  // NGInlineBoxState_h
