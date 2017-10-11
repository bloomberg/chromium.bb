// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_page_layout_algorithm.h"

#include <algorithm>
#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGPageLayoutAlgorithm::NGPageLayoutAlgorithm(NGBlockNode node,
                                             const NGConstraintSpace& space,
                                             NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

RefPtr<NGLayoutResult> NGPageLayoutAlgorithm::Layout() {
  Optional<MinMaxSize> min_max_size;
  if (NeedMinMaxSize(ConstraintSpace(), Style()))
    min_max_size = ComputeMinMaxSize();
  NGBoxStrut border_scrollbar_padding =
      CalculateBorderScrollbarPadding(ConstraintSpace(), Style(), Node());
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Style(), min_max_size);
  NGLogicalSize content_box_size =
      CalculateContentBoxSize(border_box_size, border_scrollbar_padding);
  NGLogicalSize page_size = content_box_size;

  RefPtr<NGConstraintSpace> child_space =
      CreateConstraintSpaceForPages(page_size);
  container_builder_.SetSize(border_box_size);

  NGWritingMode writing_mode = ConstraintSpace().WritingMode();
  RefPtr<NGBlockBreakToken> break_token = BreakToken();
  LayoutUnit intrinsic_block_size;
  NGLogicalOffset page_offset(border_scrollbar_padding.StartOffset());
  NGLogicalOffset page_progression;
  if (Style().OverflowY() == EOverflow::kWebkitPagedX) {
    page_progression.inline_offset = page_size.inline_size;
  } else {
    // TODO(mstensho): Handle auto block size.
    page_progression.block_offset = page_size.block_size;
  }

  do {
    // Lay out one page. Each page will become a fragment.
    NGBlockLayoutAlgorithm child_algorithm(Node(), *child_space.get(),
                                           break_token.get());
    RefPtr<NGLayoutResult> result = child_algorithm.Layout();
    RefPtr<NGPhysicalBoxFragment> page(
        ToNGPhysicalBoxFragment(result->PhysicalFragment().get()));

    container_builder_.AddChild(page, page_offset);

    NGBoxFragment logical_fragment(writing_mode, *page);
    intrinsic_block_size =
        std::max(intrinsic_block_size,
                 page_offset.block_offset + logical_fragment.BlockSize());
    page_offset += page_progression;
    break_token = ToNGBlockBreakToken(page->BreakToken());
  } while (break_token && !break_token->IsFinished());

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), intrinsic_block_size);
  container_builder_.SetBlockSize(border_box_size.block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), Style(), &container_builder_)
      .Run();

  // TODO(mstensho): Propagate baselines.

  return container_builder_.ToBoxFragment();
}

Optional<MinMaxSize> NGPageLayoutAlgorithm::ComputeMinMaxSize() const {
  return NGBlockLayoutAlgorithm(Node(), ConstraintSpace()).ComputeMinMaxSize();
}

RefPtr<NGConstraintSpace> NGPageLayoutAlgorithm::CreateConstraintSpaceForPages(
    const NGLogicalSize& page_size) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize(page_size);
  space_builder.SetPercentageResolutionSize(page_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  // TODO(mstensho): Handle auto block size. For now just disable fragmentation
  // if block size is auto. With the current approach, we'll just end up with
  // one tall page. This is only correct if there are no explicit page breaks.
  if (page_size.block_size != NGSizeIndefinite) {
    space_builder.SetFragmentationType(kFragmentPage);
    space_builder.SetFragmentainerBlockSize(page_size.block_size);
    space_builder.SetFragmentainerSpaceAtBfcStart(page_size.block_size);
  }
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace(
      FromPlatformWritingMode(Style().GetWritingMode()));
}

}  // namespace blink
