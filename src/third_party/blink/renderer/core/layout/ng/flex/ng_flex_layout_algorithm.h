// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBoxFragment;
struct DevtoolsFlexInfo;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGFlexLayoutAlgorithm(const NGLayoutAlgorithmParams& params,
                                 DevtoolsFlexInfo* devtools = nullptr);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;
  scoped_refptr<const NGLayoutResult> Layout() override;

 private:
  scoped_refptr<const NGLayoutResult> RelayoutIgnoringChildScrollbarChanges();
  scoped_refptr<const NGLayoutResult> LayoutInternal();

  Length GetUsedFlexBasis(const NGBlockNode& child) const;
  // This has an optional out parameter so that callers can avoid a subsequent
  // redundant call to GetUsedFlexBasis.
  bool IsUsedFlexBasisDefinite(const NGBlockNode& child,
                               Length* flex_basis) const;
  bool DoesItemCrossSizeComputeToAuto(const NGBlockNode& child) const;
  bool IsItemCrossAxisLengthDefinite(const NGBlockNode& child,
                                     const Length& length) const;
  bool AspectRatioProvidesMainSize(const NGBlockNode& child) const;
  bool DoesItemStretch(const NGBlockNode& child) const;
  // This checks for one of the scenarios where a flex-item box has a definite
  // size that would be indefinite if the box weren't a flex item.
  // See https://drafts.csswg.org/css-flexbox/#definite-sizes
  bool WillChildCrossSizeBeContainerCrossSize(const NGBlockNode& child) const;
  LayoutUnit AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
      const NGBlockNode& child,
      LayoutUnit content_suggestion,
      const MinMaxSizes& cross_min_max,
      const NGBoxStrut& border_padding_in_child_writing_mode);

  bool IsColumnContainerMainSizeDefinite() const;
  bool IsContainerCrossSizeDefinite() const;

  NGConstraintSpace BuildSpaceForFlexBasis(const NGBlockNode& flex_item) const;
  NGConstraintSpace BuildSpaceForIntrinsicBlockSize(
      const NGBlockNode& flex_item) const;
  // |block_offset_for_fragmentation| should only be set when running the final
  // layout pass for fragmentation.
  NGConstraintSpace BuildSpaceForLayout(
      const FlexItem& flex_item,
      absl::optional<LayoutUnit> block_offset_for_fragmentation =
          absl::nullopt) const;
  void ConstructAndAppendFlexItems();
  void ApplyStretchAlignmentToChild(FlexItem& flex_item);
  void ApplyFinalAlignmentAndReversals();
  bool GiveItemsFinalPositionAndSize();
  bool PropagateFlexItemInfo(FlexItem* flex_item,
                             wtf_size_t flex_line_idx,
                             LayoutPoint location,
                             PhysicalSize fragment_size);
  void LayoutColumnReverse(LayoutUnit main_axis_content_size);

  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(const NGBlockNode& child) const;
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size) const;

  void HandleOutOfFlowPositioned(NGBlockNode child);

  void AdjustButtonBaseline(LayoutUnit final_content_cross_size);

  // Propagates the baseline from the given flex-item if needed.
  void PropagateBaselineFromChild(
      const FlexItem&,
      const NGBoxFragment&,
      LayoutUnit block_offset,
      absl::optional<LayoutUnit>* fallback_baseline);

  // Re-layout a given flex item, taking fragmentation into account.
  void LayoutWithBlockFragmentation(FlexItem& flex_item,
                                    LayoutUnit block_offset,
                                    const NGBlockBreakToken* item_break_token);

  const bool is_column_;
  const bool is_horizontal_flow_;
  const bool is_cross_size_definite_;
  const LogicalSize child_percentage_size_;

  bool has_column_percent_flex_basis_ = false;
  bool ignore_child_scrollbar_changes_ = false;
  bool has_block_fragmentation_ = false;
  FlexLayoutAlgorithm algorithm_;
  DevtoolsFlexInfo* layout_info_for_devtools_;

  // The block size of the entire flex container (ignoring any fragmentation).
  LayoutUnit total_block_size_;
  // This will be the intrinsic block size in the current fragmentainer, if
  // inside a fragmentation context. Otherwise, it will represent the intrinsic
  // block size for the entire flex container.
  LayoutUnit intrinsic_block_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
