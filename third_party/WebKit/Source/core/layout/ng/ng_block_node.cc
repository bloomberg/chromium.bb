// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

// Copies data back to the legacy layout tree for a given child fragment.
void FragmentPositionUpdated(const NGPhysicalFragment& fragment) {
  LayoutBox* layout_box = toLayoutBox(fragment.GetLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->parent()) << "Should be called on children only.";

  // LegacyLayout flips vertical-rl horizontal coordinates before paint.
  // NGLayout flips X location for LegacyLayout compatibility.
  LayoutBlock* containing_block = layout_box->containingBlock();
  if (containing_block->styleRef().isFlippedBlocksWritingMode()) {
    LayoutUnit container_width = containing_block->size().width();
    layout_box->setX(container_width - fragment.LeftOffset() -
                     fragment.Width());
  } else {
    layout_box->setX(fragment.LeftOffset());
  }
  layout_box->setY(fragment.TopOffset());
}

// Similar to FragmentPositionUpdated but for floats.
// - Updates layout object's geometric information.
// - Creates legacy FloatingObject and attached it to the provided parent.
void FloatingObjectPositionedUpdated(NGFloatingObject* ng_floating_object,
                                     LayoutBox* parent) {
  NGPhysicalBoxFragment* box_fragment =
      toNGPhysicalBoxFragment(ng_floating_object->fragment.get());
  FragmentPositionUpdated(*box_fragment);

  LayoutBox* layout_box = toLayoutBox(box_fragment->GetLayoutObject());
  DCHECK(layout_box->isFloating());

  if (parent && parent->isLayoutBlockFlow()) {
    FloatingObject* floating_object =
        toLayoutBlockFlow(parent)->insertFloatingObject(*layout_box);
    floating_object->setX(ng_floating_object->left_offset);
    floating_object->setY(box_fragment->TopOffset());
    floating_object->setIsPlaced(true);
  }
}

}  // namespace

NGBlockNode::NGBlockNode(LayoutObject* layout_object)
    : NGLayoutInputNode(NGLayoutInputNodeType::kLegacyBlock),
      layout_box_(toLayoutBox(layout_object)) {
  DCHECK(layout_box_);
}

// Need an explicit destructor in the .cc file, or the MSWIN compiler will
// produce an error when attempting to generate a default one, if the .h file is
// included from a compilation unit that lacks the ComputedStyle definition.
NGBlockNode::~NGBlockNode() {}

RefPtr<NGLayoutResult> NGBlockNode::Layout(NGConstraintSpace* constraint_space,
                                           NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    DCHECK(layout_box_);
    layout_result_ = RunOldLayout(*constraint_space);
    return layout_result_;
  }

  layout_result_ = NGBlockLayoutAlgorithm(this, constraint_space,
                                          toNGBlockBreakToken(break_token))
                       .Layout();

  CopyFragmentDataToLayoutBox(*constraint_space);
  return layout_result_;
}

MinAndMaxContentSizes NGBlockNode::ComputeMinAndMaxContentSizes() {
  MinAndMaxContentSizes sizes;
  if (!CanUseNewLayout()) {
    DCHECK(layout_box_);
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    LayoutUnit borderAndPadding = layout_box_->borderAndPaddingLogicalWidth();
    sizes.min_content = layout_box_->computeLogicalWidthUsing(
                            MainOrPreferredSize, Length(MinContent),
                            LayoutUnit(), layout_box_->containingBlock()) -
                        borderAndPadding;
    sizes.max_content = layout_box_->computeLogicalWidthUsing(
                            MainOrPreferredSize, Length(MaxContent),
                            LayoutUnit(), layout_box_->containingBlock()) -
                        borderAndPadding;
    return sizes;
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().getWritingMode()))
          .SetTextDirection(Style().direction())
          .ToConstraintSpace(FromPlatformWritingMode(Style().getWritingMode()));

  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGBlockLayoutAlgorithm minmax_algorithm(this, constraint_space.get());
  Optional<MinAndMaxContentSizes> maybe_sizes =
      minmax_algorithm.ComputeMinAndMaxContentSizes();
  if (maybe_sizes.has_value())
    return *maybe_sizes;

  // Have to synthesize this value.
  RefPtr<NGLayoutResult> layout_result = Layout(constraint_space.get());
  NGPhysicalFragment* physical_fragment =
      layout_result->PhysicalFragment().get();
  NGBoxFragment min_fragment(FromPlatformWritingMode(Style().getWritingMode()),
                             toNGPhysicalBoxFragment(physical_fragment));
  sizes.min_content = min_fragment.InlineOverflow();

  // Now, redo with infinite space for max_content
  constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().getWritingMode()))
          .SetTextDirection(Style().direction())
          .SetAvailableSize({LayoutUnit::max(), LayoutUnit()})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .ToConstraintSpace(FromPlatformWritingMode(Style().getWritingMode()));

  layout_result = Layout(constraint_space.get());
  physical_fragment = layout_result->PhysicalFragment().get();
  NGBoxFragment max_fragment(FromPlatformWritingMode(Style().getWritingMode()),
                             toNGPhysicalBoxFragment(physical_fragment));
  sizes.max_content = max_fragment.InlineOverflow();
  return sizes;
}

const ComputedStyle& NGBlockNode::Style() const {
  if (style_)
    return *style_.get();
  DCHECK(layout_box_);
  return layout_box_->styleRef();
}

NGLayoutInputNode* NGBlockNode::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling =
        layout_box_ ? layout_box_->nextSibling() : nullptr;
    if (next_sibling) {
      if (next_sibling->isInline())
        next_sibling_ = new NGInlineNode(next_sibling, &Style());
      else
        next_sibling_ = new NGBlockNode(next_sibling);
    }
  }
  return next_sibling_;
}

LayoutObject* NGBlockNode::GetLayoutObject() {
  return layout_box_;
}

NGLayoutInputNode* NGBlockNode::FirstChild() {
  if (!first_child_) {
    LayoutObject* child = layout_box_ ? layout_box_->slowFirstChild() : nullptr;
    if (child) {
      if (child->isInline()) {
        first_child_ = new NGInlineNode(child, &Style());
      } else {
        first_child_ = new NGBlockNode(child);
      }
    }
  }
  return first_child_;
}

DEFINE_TRACE(NGBlockNode) {
  visitor->trace(next_sibling_);
  visitor->trace(first_child_);
  NGLayoutInputNode::trace(visitor);
}

bool NGBlockNode::CanUseNewLayout() {
  if (!layout_box_)
    return true;
  if (!layout_box_->isLayoutBlockFlow())
    return false;
  return RuntimeEnabledFeatures::layoutNGInlineEnabled() ||
         !HasInlineChildren();
}

bool NGBlockNode::HasInlineChildren() {
  if (!layout_box_ || !layout_box_->isLayoutBlockFlow())
    return false;

  const LayoutBlockFlow* block_flow = toLayoutBlockFlow(layout_box_);
  if (!block_flow->childrenInline())
    return false;
  LayoutObject* child = block_flow->firstChild();
  while (child) {
    if (child->isInline())
      return true;
    child = child->nextSibling();
  }

  return false;
}

void NGBlockNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space) {
  // We may not have a layout_box_ during unit tests.
  if (!layout_box_)
    return;

  NGPhysicalBoxFragment* fragment =
      toNGPhysicalBoxFragment(layout_result_->PhysicalFragment().get());

  layout_box_->setWidth(fragment->Width());
  layout_box_->setHeight(fragment->Height());
  NGBoxStrut border_and_padding = ComputeBorders(constraint_space, Style()) +
                                  ComputePadding(constraint_space, Style());
  LayoutUnit intrinsic_logical_height =
      layout_box_->style()->isHorizontalWritingMode()
          ? fragment->HeightOverflow()
          : fragment->WidthOverflow();
  intrinsic_logical_height -= border_and_padding.BlockSum();
  layout_box_->setIntrinsicContentLogicalHeight(intrinsic_logical_height);

  // We may still have unpositioned floats when we reach the root box.
  if (!layout_box_->parent()) {
    for (const auto& floating_object : fragment->PositionedFloats()) {
      FloatingObjectPositionedUpdated(floating_object, layout_box_);
    }
  }

  // TODO(layout-dev): Currently we are not actually performing layout on
  // inline children. For now just clear the needsLayout bit so that we can
  // run unittests.
  if (HasInlineChildren()) {
    for (InlineWalker walker(
             LineLayoutBlockFlow(toLayoutBlockFlow(layout_box_)));
         !walker.atEnd(); walker.advance()) {
      LayoutObject* o = LineLayoutAPIShim::layoutObjectFrom(walker.current());
      o->clearNeedsLayout();
    }

    // Ensure the position of the children are copied across to the
    // LayoutObject tree.
  } else {
    for (const auto& child_fragment : fragment->Children()) {
      if (child_fragment->IsPlaced())
        FragmentPositionUpdated(toNGPhysicalBoxFragment(*child_fragment));

      for (const auto& floating_object :
           toNGPhysicalBoxFragment(child_fragment.get())->PositionedFloats()) {
        FloatingObjectPositionedUpdated(
            floating_object, toLayoutBox(child_fragment->GetLayoutObject()));
      }
    }
  }

  if (layout_box_->isLayoutBlock())
    toLayoutBlock(layout_box_)->layoutPositionedObjects(true);
  layout_box_->clearNeedsLayout();
  if (layout_box_->isLayoutBlockFlow()) {
    toLayoutBlockFlow(layout_box_)->updateIsSelfCollapsing();
  }
}

RefPtr<NGLayoutResult> NGBlockNode::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  NGLogicalSize available_size = constraint_space.PercentageResolutionSize();
  LayoutObject* containing_block = layout_box_->containingBlock();
  bool parallel_writing_mode;
  if (!containing_block) {
    parallel_writing_mode = true;
  } else {
    parallel_writing_mode = IsParallelWritingMode(
        FromPlatformWritingMode(containing_block->styleRef().getWritingMode()),
        FromPlatformWritingMode(Style().getWritingMode()));
  }
  if (parallel_writing_mode) {
    layout_box_->setOverrideContainingBlockContentLogicalWidth(
        available_size.inline_size);
    layout_box_->setOverrideContainingBlockContentLogicalHeight(
        available_size.block_size);
  } else {
    // OverrideContainingBlock should be in containing block writing mode.
    layout_box_->setOverrideContainingBlockContentLogicalWidth(
        available_size.block_size);
    layout_box_->setOverrideContainingBlockContentLogicalHeight(
        available_size.inline_size);
  }
  // TODO(layout-ng): Does this handle scrollbars correctly?
  if (constraint_space.IsFixedSizeInline()) {
    layout_box_->setOverrideLogicalContentWidth(
        constraint_space.AvailableSize().inline_size -
        layout_box_->borderAndPaddingLogicalWidth());
  }
  if (constraint_space.IsFixedSizeBlock()) {
    layout_box_->setOverrideLogicalContentHeight(
        constraint_space.AvailableSize().block_size -
        layout_box_->borderAndPaddingLogicalHeight());
  }

  if (layout_box_->isLayoutNGBlockFlow() && layout_box_->needsLayout()) {
    toLayoutNGBlockFlow(layout_box_)->LayoutBlockFlow::layoutBlock(true);
  } else {
    layout_box_->forceLayout();
  }
  LayoutRect overflow = layout_box_->layoutOverflowRect();
  // TODO(layout-ng): This does not handle writing modes correctly (for
  // overflow)
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);
  builder.SetInlineSize(layout_box_->logicalWidth())
      .SetBlockSize(layout_box_->logicalHeight())
      .SetDirection(layout_box_->styleRef().direction())
      .SetWritingMode(
          FromPlatformWritingMode(layout_box_->styleRef().getWritingMode()))
      .SetInlineOverflow(overflow.width())
      .SetBlockOverflow(overflow.height());
  return builder.ToBoxFragment();
}

void NGBlockNode::UseOldOutOfFlowPositioning() {
  DCHECK(layout_box_);
  DCHECK(layout_box_->isOutOfFlowPositioned());
  layout_box_->containingBlock()->insertPositionedObject(layout_box_);
}

// Save static position for legacy AbsPos layout.
void NGBlockNode::SaveStaticOffsetForLegacy(const NGLogicalOffset& offset) {
  DCHECK(layout_box_);
  DCHECK(layout_box_->isOutOfFlowPositioned());
  DCHECK(layout_box_->layer());
  layout_box_->layer()->setStaticBlockPosition(offset.block_offset);
  layout_box_->layer()->setStaticInlinePosition(offset.inline_offset);
}

}  // namespace blink
