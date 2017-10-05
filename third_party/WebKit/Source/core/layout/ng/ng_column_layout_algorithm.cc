// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_column_layout_algorithm.h"

#include <algorithm>
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(NGBlockNode node,
                                                 const NGConstraintSpace& space,
                                                 NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

RefPtr<NGLayoutResult> NGColumnLayoutAlgorithm::Layout() {
  Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxSize();
  NGBoxStrut border_scrollbar_padding =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Style(), Node());
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Style(), min_max_size);
  content_box_size_ =
      CalculateContentBoxSize(border_box_size, border_scrollbar_padding);

  RefPtr<NGConstraintSpace> child_space = CreateConstraintSpaceForColumns();
  if (border_box_size.block_size == NGSizeIndefinite) {
    // Get the block size from the columns if it's auto.
    border_box_size.block_size = child_space->AvailableSize().block_size +
                                 border_scrollbar_padding.BlockSum();
  }
  container_builder_.SetSize(border_box_size);

  NGWritingMode writing_mode = ConstraintSpace().WritingMode();
  RefPtr<NGBlockBreakToken> break_token = BreakToken();
  LayoutUnit intrinsic_block_size;
  LayoutUnit column_inline_offset(border_scrollbar_padding.inline_start);
  LayoutUnit column_block_offset(border_scrollbar_padding.block_start);
  LayoutUnit column_inline_progression =
      child_space->AvailableSize().inline_size + ResolveUsedColumnGap(Style());

  do {
    // Lay out one column. Each column will become a fragment.
    NGBlockLayoutAlgorithm child_algorithm(Node(), *child_space.get(),
                                           break_token.get());
    RefPtr<NGLayoutResult> result = child_algorithm.Layout();
    RefPtr<NGPhysicalBoxFragment> column(
        ToNGPhysicalBoxFragment(result->PhysicalFragment().get()));

    NGLogicalOffset logical_offset(column_inline_offset, column_block_offset);
    container_builder_.AddChild(column, logical_offset);

    intrinsic_block_size = std::max(
        intrinsic_block_size,
        column_block_offset + NGBoxFragment(writing_mode, *column).BlockSize());

    column_inline_offset += column_inline_progression;
    break_token = ToNGBlockBreakToken(column->BreakToken());
  } while (break_token && !break_token->IsFinished());

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), Style(), &container_builder_)
      .Run();

  // TODO(mstensho): Propagate baselines.

  return container_builder_.ToBoxFragment();
}

Optional<MinMaxSize> NGColumnLayoutAlgorithm::ComputeMinMaxSize() const {
  // First calculate the min/max sizes of columns.
  Optional<MinMaxSize> min_max_sizes =
      NGBlockLayoutAlgorithm(Node(), ConstraintSpace()).ComputeMinMaxSize();
  DCHECK(min_max_sizes.has_value());
  MinMaxSize sizes = min_max_sizes.value();

  // If column-width is non-auto, pick the larger of that and intrinsic column
  // width.
  if (!Style().HasAutoColumnWidth()) {
    sizes.min_size =
        std::max(sizes.min_size, LayoutUnit(Style().ColumnWidth()));
    sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  }

  // Now convert those column min/max values to multicol container min/max
  // values. We typically have multiple columns and also gaps between them.
  int column_count = Style().ColumnCount();
  DCHECK_GE(column_count, 1);
  sizes.min_size *= column_count;
  sizes.max_size *= column_count;
  LayoutUnit column_gap = ResolveUsedColumnGap(Style());
  LayoutUnit gap_extra = column_gap * (column_count - 1);
  sizes.min_size += gap_extra;
  sizes.max_size += gap_extra;

  return sizes;
}

RefPtr<NGConstraintSpace>
NGColumnLayoutAlgorithm::CreateConstraintSpaceForColumns() const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  NGLogicalSize adjusted_size = content_box_size_;
  adjusted_size.inline_size =
      ResolveUsedColumnInlineSize(adjusted_size.inline_size, Style());

  // TODO(mstensho): Implement column balancing. For now just set it to some
  // arbitrary positive length if height is auto, so that we don't get stuck in
  // infinite loops, because of no block progression (negative, even) between
  // one fragmentainer and the next.
  if (adjusted_size.block_size == NGSizeIndefinite)
    adjusted_size.block_size = LayoutUnit(42);
  // To ensure progression, we need something larger than 0 here. The spec
  // actually says that fragmentainers have to accept at least 1px of content.
  adjusted_size.block_size = std::max(adjusted_size.block_size, LayoutUnit(1));

  space_builder.SetAvailableSize(adjusted_size);
  space_builder.SetPercentageResolutionSize(adjusted_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(adjusted_size.block_size);
  space_builder.SetFragmentainerSpaceAtBfcStart(adjusted_size.block_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace(
      FromPlatformWritingMode(Style().GetWritingMode()));
}

}  // namespace Blink
