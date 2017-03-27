// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineLayoutAlgorithm_h
#define NGInlineLayoutAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/ng/geometry/ng_logical_offset.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_algorithm.h"
#include "core/layout/ng/ng_line_height_metrics.h"
#include "platform/fonts/FontBaseline.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class NGConstraintSpace;
class NGInlineBreakToken;
class NGInlineNode;
class NGLayoutInlineItem;
class NGLineBoxFragmentBuilder;
class NGTextFragmentBuilder;

// A class for inline layout (e.g. a <span> with no special style).
//
// Uses NGLineBreaker to find break opportunities, and let it call back to
// construct linebox fragments and its wrapper box fragment.
//
// From a line breaker, this class manages the current line as a range, |start|
// and |end|. |end| can be extended multiple times before creating a line,
// usually until |!CanFitOnLine()|. |SetBreakOpportunity| can mark the last
// confirmed offset that can fit.
class CORE_EXPORT NGInlineLayoutAlgorithm final : public NGLayoutAlgorithm {
 public:
  NGInlineLayoutAlgorithm(NGInlineNode*,
                          NGConstraintSpace*,
                          NGInlineBreakToken* = nullptr);

  const NGConstraintSpace& ConstraintSpace() const {
    return *constraint_space_;
  }

  LayoutUnit MaxInlineSize() const { return max_inline_size_; }

  // Returns if the current items fit on a line.
  bool CanFitOnLine() const;

  // Returns if there were any items.
  bool HasItems() const;

  // Set the end offset.
  void SetEnd(unsigned end_offset);

  // Set the end offset if caller knows the inline size since the current end.
  void SetEnd(unsigned index,
              unsigned end_offset,
              LayoutUnit inline_size_since_current_end);

  // Create a line up to the end offset.
  // Then set the start to the end offset, and thus empty the current line.
  // @return false if the line does not fit in the constraint space in block
  //         direction.
  bool CreateLine();

  // Returns if a break opportunity was set on the current line.
  bool HasBreakOpportunity() const;

  // Returns if there were items after the last break opportunity.
  bool HasItemsAfterLastBreakOpportunity() const;

  // Set the break opportunity at the current end offset.
  void SetBreakOpportunity();

  // Create a line up to the last break opportunity.
  // Items after that are sent to the next line.
  // @return false if the line does not fit in the constraint space in block
  //         direction.
  bool CreateLineUpToLastBreakOpportunity();

  // Set the start offset of hangables; e.g., spaces or hanging punctuations.
  // Hangable characters can go beyond the right margin, and are ignored for
  // center/right alignment.
  void SetStartOfHangables(unsigned offset);

  RefPtr<NGLayoutResult> Layout() override;

  // Compute MinMaxContentSize by performing layout.
  // Unlike NGLayoutAlgorithm::ComputeMinMaxContentSize(), this function runs
  // part of layout operations and modifies the state of |this|.
  MinMaxContentSize ComputeMinMaxContentSizeByLayout();

  // Copy fragment data of all lines to LayoutBlockFlow.
  // TODO(kojii): Move to NGInlineNode (or remove when paint is implemented.)
  void CopyFragmentDataToLayoutBlockFlow(NGLayoutResult*);

  // Compute inline size of an NGLayoutInlineItem.
  // Same as NGLayoutInlineItem::InlineSize(), except that this function can
  // compute atomic inlines by performing layout.
  LayoutUnit InlineSize(const NGLayoutInlineItem&);

 private:
  bool IsHorizontalWritingMode() const { return is_horizontal_writing_mode_; }

  // Set the start and the end to the specified offset.
  // This empties the current line.
  void Initialize(unsigned index, unsigned offset);

  LayoutUnit InlineSize(const NGLayoutInlineItem&,
                        unsigned start_offset,
                        unsigned end_offset);
  LayoutUnit InlineSizeFromLayout(const NGLayoutInlineItem&);
  const NGLayoutResult* LayoutItem(const NGLayoutInlineItem&);

  struct LineItemChunk {
    unsigned index;
    unsigned start_offset;
    unsigned end_offset;
    LayoutUnit inline_size;
  };

  void BidiReorder(Vector<LineItemChunk, 32>*);

  // Lays out the inline float.
  // List of actions:
  // - tries to position the float right away if we have enough space.
  // - updates the current_opportunity if we actually place the float.
  // - if it's too wide then we add the float to the unpositioned list so it can
  //   be positioned after we're done with the current line.
  void LayoutAndPositionFloat(LayoutUnit end_position, LayoutObject*);

  bool PlaceItems(const Vector<LineItemChunk, 32>&);
  void AccumulateUsedFonts(const NGLayoutInlineItem&,
                           const LineItemChunk&,
                           NGLineBoxFragmentBuilder*);
  LayoutUnit PlaceAtomicInline(const NGLayoutInlineItem&,
                               LayoutUnit estimated_baseline,
                               NGLineBoxFragmentBuilder*,
                               NGTextFragmentBuilder*);

  // Finds the next layout opportunity for the next text fragment.
  void FindNextLayoutOpportunity();

  Persistent<NGInlineNode> inline_box_;
  NGConstraintSpace* constraint_space_;  // Not owned as STACK_ALLOCATED.
  Vector<RefPtr<NGLayoutResult>, 32> layout_results_;
  unsigned start_index_ = 0;
  unsigned start_offset_ = 0;
  unsigned last_index_ = 0;
  unsigned end_offset_ = 0;
  unsigned last_break_opportunity_index_ = 0;
  unsigned last_break_opportunity_offset_ = 0;
  LayoutUnit end_position_;
  LayoutUnit last_break_opportunity_position_;
  LayoutUnit content_size_;
  LayoutUnit max_inline_size_;
  NGFragmentBuilder container_builder_;
  FontBaseline baseline_type_ = FontBaseline::AlphabeticBaseline;

  NGLogicalOffset bfc_offset_;
  NGLogicalRect current_opportunity_;

  unsigned is_horizontal_writing_mode_ : 1;

  NGConstraintSpaceBuilder space_builder_;
#if DCHECK_IS_ON()
  unsigned is_bidi_reordered_ : 1;
#endif
};

}  // namespace blink

#endif  // NGInlineLayoutAlgorithm_h
