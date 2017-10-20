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

namespace {

inline bool NeedsColumnBalancing(LayoutUnit block_size,
                                 const ComputedStyle& style) {
  return block_size == NGSizeIndefinite ||
         style.GetColumnFill() == EColumnFill::kBalance;
}

// Constrain a balanced column block size to not overflow the multicol
// container.
LayoutUnit ConstrainColumnBlockSize(LayoutUnit size,
                                    NGBlockNode node,
                                    const NGConstraintSpace& space,
                                    const ComputedStyle& style) {
  // The {,max-}{height,width} properties are specified on the multicol
  // container, but here we're calculating the column block sizes inside the
  // multicol container, which isn't exactly the same. We may shrink the column
  // block size here, but we'll never stretch it, because the value passed is
  // the perfect balanced block size. Making it taller would only disrupt the
  // balanced output, for no reason. The only thing we need to worry about here
  // is to not overflow the multicol container.

  // First of all we need to convert the size to a value that can be compared
  // against the resolved properties on the multicol container. That means that
  // we have to convert the value from content-box to border-box.
  NGBoxStrut border_scrollbar_padding =
      CalculateBorderScrollbarPadding(space, style, node);
  LayoutUnit extra = border_scrollbar_padding.BlockSum();
  size += extra;

  Optional<LayoutUnit> max_length;
  Length logical_max_height = style.LogicalMaxHeight();
  if (!logical_max_height.IsMaxSizeNone()) {
    max_length = ResolveBlockLength(space, style, logical_max_height, size,
                                    LengthResolveType::kMaxSize);
  }
  LayoutUnit extent = ResolveBlockLength(space, style, style.LogicalHeight(),
                                         size, LengthResolveType::kContentSize);
  if (extent != NGSizeIndefinite) {
    // A specified height/width will just constrain the maximum length.
    if (max_length.has_value())
      max_length = std::min(max_length.value(), extent);
    else
      max_length = extent;
  }

  // Constrain and convert the value back to content-box.
  if (max_length.has_value())
    size = std::min(size, max_length.value());
  return size - extra;
}

}  // namespace

NGColumnLayoutAlgorithm::NGColumnLayoutAlgorithm(NGBlockNode node,
                                                 const NGConstraintSpace& space,
                                                 NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

scoped_refptr<NGLayoutResult> NGColumnLayoutAlgorithm::Layout() {
  Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxSize();
  NGBoxStrut border_scrollbar_padding =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Style(), Node());
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Style(), min_max_size);
  NGLogicalSize content_box_size =
      CalculateContentBoxSize(border_box_size, border_scrollbar_padding);
  NGLogicalSize column_size = CalculateColumnSize(content_box_size);

  scoped_refptr<NGConstraintSpace> child_space =
      CreateConstraintSpaceForColumns(column_size);
  if (border_box_size.block_size == NGSizeIndefinite) {
    // Get the block size from the columns if it's auto.
    border_box_size.block_size =
        column_size.block_size + border_scrollbar_padding.BlockSum();
  }
  container_builder_.SetInlineSize(border_box_size.inline_size);
  container_builder_.SetBlockSize(border_box_size.block_size);

  NGWritingMode writing_mode = ConstraintSpace().WritingMode();
  scoped_refptr<NGBlockBreakToken> break_token = BreakToken();
  LayoutUnit intrinsic_block_size;
  LayoutUnit column_inline_offset(border_scrollbar_padding.inline_start);
  LayoutUnit column_block_offset(border_scrollbar_padding.block_start);
  LayoutUnit column_inline_progression =
      child_space->AvailableSize().inline_size + ResolveUsedColumnGap(Style());

  do {
    // Lay out one column. Each column will become a fragment.
    NGBlockLayoutAlgorithm child_algorithm(Node(), *child_space.get(),
                                           break_token.get());
    scoped_refptr<NGLayoutResult> result = child_algorithm.Layout();
    scoped_refptr<NGPhysicalBoxFragment> column(
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

NGLogicalSize NGColumnLayoutAlgorithm::CalculateColumnSize(
    const NGLogicalSize& content_box_size) {
  NGLogicalSize column_size = content_box_size;
  DCHECK_GE(column_size.inline_size, LayoutUnit());
  column_size.inline_size =
      ResolveUsedColumnInlineSize(column_size.inline_size, Style());

  if (NeedsColumnBalancing(column_size.block_size, Style())) {
    int used_count =
        ResolveUsedColumnCount(content_box_size.inline_size, Style());
    column_size.block_size =
        CalculateBalancedColumnBlockSize(column_size, used_count);
  }

  return column_size;
}

LayoutUnit NGColumnLayoutAlgorithm::CalculateBalancedColumnBlockSize(
    const NGLogicalSize& column_size,
    int column_count) {
  // To calculate a balanced column size, we need to figure out how tall our
  // content is. To do that we need to lay out. Create a special constraint
  // space for column balancing, without splitting into fragmentainers. It will
  // make us lay out all the multicol content as one single tall strip. When
  // we're done with this layout pass, we can examine the result and calculate
  // an ideal column block size.
  auto space = CreateConstaintSpaceForBalancing(column_size);
  NGBlockLayoutAlgorithm balancing_algorithm(Node(), *space.get());
  scoped_refptr<NGLayoutResult> result = balancing_algorithm.Layout();

  // TODO(mstensho): This is where the fun begins. We need to examine the entire
  // fragment tree, not just the root.
  NGFragment fragment(space->WritingMode(), *result->PhysicalFragment().get());
  LayoutUnit single_strip_block_size = fragment.BlockSize();

  // Some extra care is required the division here. We want a the resulting
  // LayoutUnit value to be large enough to prevent overflowing columns. Use
  // floating point to get higher precision than LayoutUnit. Then convert it to
  // a LayoutUnit, but round it up to the nearest value that LayoutUnit is able
  // to represent.
  LayoutUnit block_size = LayoutUnit::FromFloatCeil(
      single_strip_block_size.ToFloat() / static_cast<float>(column_count));

  // Finally, honor {,min-,max-}{height,width} properties.
  return ConstrainColumnBlockSize(block_size, Node(), ConstraintSpace(),
                                  Style());
}

scoped_refptr<NGConstraintSpace>
NGColumnLayoutAlgorithm::CreateConstraintSpaceForColumns(
    const NGLogicalSize& column_size) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize(column_size);
  space_builder.SetPercentageResolutionSize(column_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  // To ensure progression, we need something larger than 0 here. The spec
  // actually says that fragmentainers have to accept at least 1px of content.
  // See https://www.w3.org/TR/css-break-3/#breaking-rules
  LayoutUnit column_block_size =
      std::max(column_size.block_size, LayoutUnit(1));

  space_builder.SetFragmentationType(kFragmentColumn);
  space_builder.SetFragmentainerBlockSize(column_block_size);
  space_builder.SetFragmentainerSpaceAtBfcStart(column_block_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace(
      FromPlatformWritingMode(Style().GetWritingMode()));
}

scoped_refptr<NGConstraintSpace>
NGColumnLayoutAlgorithm::CreateConstaintSpaceForBalancing(
    const NGLogicalSize& column_size) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize({column_size.inline_size, NGSizeIndefinite});
  space_builder.SetPercentageResolutionSize(column_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace(
      FromPlatformWritingMode(Style().GetWritingMode()));
}

}  // namespace Blink
