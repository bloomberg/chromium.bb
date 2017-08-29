// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_layout_algorithm_test.h"

#include "core/dom/Element.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

NGBaseLayoutAlgorithmTest::NGBaseLayoutAlgorithmTest() {
  RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
}

NGBaseLayoutAlgorithmTest::~NGBaseLayoutAlgorithmTest() {
  RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
}

void NGBaseLayoutAlgorithmTest::SetUp() {
  RenderingTest::SetUp();
  EnableCompositing();
}

std::pair<RefPtr<NGPhysicalBoxFragment>, RefPtr<NGConstraintSpace>>
NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithmForElement(Element* element) {
  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(element->GetLayoutObject());
  NGBlockNode node(block_flow);
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);

  RefPtr<NGLayoutResult> result = NGBlockLayoutAlgorithm(node, *space).Layout();
  return std::make_pair(
      ToNGPhysicalBoxFragment(result->PhysicalFragment().Get()),
      std::move(space));
}

const NGPhysicalBoxFragment* FragmentChildIterator::NextChild() {
  if (!parent_)
    return nullptr;
  if (index_ >= parent_->Children().size())
    return nullptr;
  while (parent_->Children()[index_]->Type() !=
         NGPhysicalFragment::kFragmentBox) {
    ++index_;
    if (index_ >= parent_->Children().size())
      return nullptr;
  }
  return ToNGPhysicalBoxFragment(parent_->Children()[index_++].Get());
}

RefPtr<NGConstraintSpace> ConstructBlockLayoutTestConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize size,
    bool shrink_to_fit,
    bool is_new_formatting_context,
    LayoutUnit fragmentainer_space_available) {
  NGFragmentationType block_fragmentation =
      fragmentainer_space_available != LayoutUnit()
          ? NGFragmentationType::kFragmentColumn
          : NGFragmentationType::kFragmentNone;

  return NGConstraintSpaceBuilder(
             writing_mode,
             /* icb_size */ size.ConvertToPhysical(writing_mode))
      .SetAvailableSize(size)
      .SetPercentageResolutionSize(size)
      .SetTextDirection(direction)
      .SetIsShrinkToFit(shrink_to_fit)
      .SetIsNewFormattingContext(is_new_formatting_context)
      .SetFragmentainerSpaceAvailable(fragmentainer_space_available)
      .SetFragmentationType(block_fragmentation)
      .ToConstraintSpace(writing_mode);
}

}  // namespace blink
