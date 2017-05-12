// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/line/InlineIterator.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_column_layout_algorithm.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_min_max_content_size.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

RefPtr<NGLayoutResult> LayoutWithAlgorithm(const ComputedStyle& style,
                                           NGBlockNode* node,
                                           NGConstraintSpace* space,
                                           NGBreakToken* break_token) {
  if (style.SpecifiesColumns())
    return NGColumnLayoutAlgorithm(node, space,
                                   ToNGBlockBreakToken(break_token))
        .Layout();
  return NGBlockLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token))
      .Layout();
}

// Copies data back to the legacy layout tree for a given child fragment.
void FragmentPositionUpdated(const NGPhysicalFragment& fragment) {
  LayoutBox* layout_box = ToLayoutBox(fragment.GetLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  // LegacyLayout flips vertical-rl horizontal coordinates before paint.
  // NGLayout flips X location for LegacyLayout compatibility.
  LayoutBlock* containing_block = layout_box->ContainingBlock();
  if (containing_block->StyleRef().IsFlippedBlocksWritingMode()) {
    LayoutUnit container_width = containing_block->Size().Width();
    layout_box->SetX(container_width - fragment.LeftOffset() -
                     fragment.Width());
  } else {
    layout_box->SetX(fragment.LeftOffset());
  }
  layout_box->SetY(fragment.TopOffset());
}

// Similar to FragmentPositionUpdated but for floats.
// - Updates layout object's geometric information.
// - Creates legacy FloatingObject and attached it to the provided parent.
void FloatingObjectPositionedUpdated(const NGPositionedFloat& positioned_float,
                                     LayoutBox* parent) {
  NGPhysicalBoxFragment* box_fragment =
      ToNGPhysicalBoxFragment(positioned_float.fragment.Get());
  FragmentPositionUpdated(*box_fragment);

  LayoutBox* layout_box = ToLayoutBox(box_fragment->GetLayoutObject());
  DCHECK(layout_box->IsFloating());

  if (parent && parent->IsLayoutBlockFlow()) {
    FloatingObject* floating_object =
        ToLayoutBlockFlow(parent)->InsertFloatingObject(*layout_box);
    floating_object->SetIsInPlacedTree(false);
    floating_object->SetX(positioned_float.paint_offset.left);
    floating_object->SetY(positioned_float.paint_offset.top);
    floating_object->SetIsPlaced(true);
    floating_object->SetIsInPlacedTree(true);
  }
}

void UpdateLegacyMultiColumnFlowThread(LayoutBox* layout_box,
                                       const NGPhysicalBoxFragment* fragment) {
  LayoutBlockFlow* multicol = ToLayoutBlockFlow(layout_box);
  LayoutMultiColumnFlowThread* flow_thread = multicol->MultiColumnFlowThread();
  if (!flow_thread)
    return;
  if (LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet()) {
    column_set->SetWidth(fragment->Width());
    column_set->SetHeight(fragment->Height());

    // TODO(mstensho): This value has next to nothing to do with the flow thread
    // portion size, but at least it's usually better than zero.
    column_set->EndFlow(fragment->Height());

    column_set->ClearNeedsLayout();
  }
  // TODO(mstensho): Fix the relatively nonsensical values here (the content box
  // size of the multicol container has very little to do with the price of
  // eggs).
  flow_thread->SetWidth(fragment->Width());
  flow_thread->SetHeight(fragment->Height());

  flow_thread->ValidateColumnSets();
  flow_thread->ClearNeedsLayout();
}

}  // namespace

NGBlockNode::NGBlockNode(LayoutObject* layout_object)
    : NGLayoutInputNode(NGLayoutInputNodeType::kLegacyBlock),
      layout_box_(ToLayoutBox(layout_object)) {
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
    return RunOldLayout(*constraint_space);
  }

  RefPtr<NGLayoutResult> layout_result =
      LayoutWithAlgorithm(Style(), this, constraint_space, break_token);

  CopyFragmentDataToLayoutBox(*constraint_space, layout_result.Get());
  return layout_result;
}

MinMaxContentSize NGBlockNode::ComputeMinMaxContentSize() {
  MinMaxContentSize sizes;
  if (!CanUseNewLayout()) {
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    LayoutUnit border_and_padding = layout_box_->BorderAndPaddingLogicalWidth();
    sizes.min_content = layout_box_->ComputeLogicalWidthUsing(
                            kMainOrPreferredSize, Length(kMinContent),
                            LayoutUnit(), layout_box_->ContainingBlock()) -
                        border_and_padding;
    sizes.max_content = layout_box_->ComputeLogicalWidthUsing(
                            kMainOrPreferredSize, Length(kMaxContent),
                            LayoutUnit(), layout_box_->ContainingBlock()) -
                        border_and_padding;
    return sizes;
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().GetWritingMode()))
          .SetTextDirection(Style().Direction())
          .ToConstraintSpace(FromPlatformWritingMode(Style().GetWritingMode()));

  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGBlockLayoutAlgorithm minmax_algorithm(this, constraint_space.Get());
  Optional<MinMaxContentSize> maybe_sizes =
      minmax_algorithm.ComputeMinMaxContentSize();
  if (maybe_sizes.has_value())
    return *maybe_sizes;

  // Have to synthesize this value.
  RefPtr<NGLayoutResult> layout_result = Layout(constraint_space.Get());
  NGPhysicalFragment* physical_fragment =
      layout_result->PhysicalFragment().Get();
  NGBoxFragment min_fragment(FromPlatformWritingMode(Style().GetWritingMode()),
                             ToNGPhysicalBoxFragment(physical_fragment));
  sizes.min_content = min_fragment.OverflowSize().inline_size;

  // Now, redo with infinite space for max_content
  constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().GetWritingMode()))
          .SetTextDirection(Style().Direction())
          .SetAvailableSize({LayoutUnit::Max(), LayoutUnit()})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .ToConstraintSpace(FromPlatformWritingMode(Style().GetWritingMode()));

  layout_result = Layout(constraint_space.Get());
  physical_fragment = layout_result->PhysicalFragment().Get();
  NGBoxFragment max_fragment(FromPlatformWritingMode(Style().GetWritingMode()),
                             ToNGPhysicalBoxFragment(physical_fragment));
  sizes.max_content = max_fragment.OverflowSize().inline_size;
  return sizes;
}

const ComputedStyle& NGBlockNode::Style() const {
  return layout_box_->StyleRef();
}

NGLayoutInputNode* NGBlockNode::NextSibling() {
  if (!next_sibling_) {
    LayoutObject* next_sibling = layout_box_->NextSibling();
    if (next_sibling) {
      if (next_sibling->IsInline()) {
        // As long as we traverse LayoutObject tree, this should not happen.
        // See ShouldHandleByInlineContext() for more context.
        // Also this leads to incorrect layout because we create two
        // NGLayoutInputNode for one LayoutBlockFlow.
        NOTREACHED();
        next_sibling_ = new NGInlineNode(
            next_sibling, ToLayoutNGBlockFlow(layout_box_->Parent()));
      } else {
        next_sibling_ = new NGBlockNode(next_sibling);
      }
    }
  }
  return next_sibling_;
}

LayoutObject* NGBlockNode::GetLayoutObject() const {
  return layout_box_;
}

NGLayoutInputNode* NGBlockNode::FirstChild() {
  if (!first_child_) {
    LayoutObject* child = layout_box_->SlowFirstChild();
    if (child) {
      if (layout_box_->ChildrenInline()) {
        first_child_ =
            new NGInlineNode(child, ToLayoutNGBlockFlow(layout_box_));
      } else {
        first_child_ = new NGBlockNode(child);
      }
    }
  }
  return first_child_;
}

DEFINE_TRACE(NGBlockNode) {
  visitor->Trace(next_sibling_);
  visitor->Trace(first_child_);
  NGLayoutInputNode::Trace(visitor);
}

bool NGBlockNode::CanUseNewLayout() const {
  // [Multicol]: for the 1st phase of LayoutNG's multicol implementation we want
  // to utilize the existing ColumnBalancer class. That's why a multicol block
  // should be processed by Legacy Layout engine.
  if (Style().SpecifiesColumns())
    return false;

  if (!layout_box_->IsLayoutBlockFlow())
    return false;
  return RuntimeEnabledFeatures::layoutNGEnabled();
}

String NGBlockNode::ToString() const {
  return String::Format("NGBlockNode: '%s'",
                        GetLayoutObject()->DebugName().Ascii().data());
}

void NGBlockNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    NGLayoutResult* layout_result) {
  NGPhysicalBoxFragment* fragment =
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get());

  if (layout_box_->Style()->SpecifiesColumns())
    UpdateLegacyMultiColumnFlowThread(layout_box_, fragment);
  layout_box_->SetWidth(fragment->Width());
  layout_box_->SetHeight(fragment->Height());
  NGBoxStrut border_and_padding = ComputeBorders(constraint_space, Style()) +
                                  ComputePadding(constraint_space, Style());
  LayoutUnit intrinsic_logical_height =
      layout_box_->Style()->IsHorizontalWritingMode()
          ? fragment->OverflowSize().height
          : fragment->OverflowSize().width;
  intrinsic_logical_height -= border_and_padding.BlockSum();
  layout_box_->SetIntrinsicContentLogicalHeight(intrinsic_logical_height);

  // We may still have unpositioned floats when we reach the root box.
  if (!layout_box_->Parent()) {
    for (const NGPositionedFloat& positioned_float :
         fragment->PositionedFloats()) {
      FloatingObjectPositionedUpdated(positioned_float, layout_box_);
    }
  }

  for (const auto& child_fragment : fragment->Children()) {
    if (child_fragment->IsPlaced())
      FragmentPositionUpdated(ToNGPhysicalBoxFragment(*child_fragment));

    for (const NGPositionedFloat& positioned_float :
         ToNGPhysicalBoxFragment(child_fragment.Get())->PositionedFloats()) {
      FloatingObjectPositionedUpdated(
          positioned_float, ToLayoutBox(child_fragment->GetLayoutObject()));
    }
  }

  if (layout_box_->IsLayoutBlock())
    ToLayoutBlock(layout_box_)->LayoutPositionedObjects(true);
  layout_box_->ClearNeedsLayout();
  if (layout_box_->IsLayoutBlockFlow()) {
    ToLayoutBlockFlow(layout_box_)->UpdateIsSelfCollapsing();
  }
}

RefPtr<NGLayoutResult> NGBlockNode::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  NGLogicalSize available_size = constraint_space.PercentageResolutionSize();
  LayoutObject* containing_block = layout_box_->ContainingBlock();
  NGWritingMode writing_mode =
      FromPlatformWritingMode(Style().GetWritingMode());
  bool parallel_writing_mode;
  if (!containing_block) {
    parallel_writing_mode = true;
  } else {
    parallel_writing_mode = IsParallelWritingMode(
        FromPlatformWritingMode(containing_block->StyleRef().GetWritingMode()),
        writing_mode);
  }
  if (parallel_writing_mode) {
    layout_box_->SetOverrideContainingBlockContentLogicalWidth(
        available_size.inline_size);
    layout_box_->SetOverrideContainingBlockContentLogicalHeight(
        available_size.block_size);
  } else {
    // OverrideContainingBlock should be in containing block writing mode.
    layout_box_->SetOverrideContainingBlockContentLogicalWidth(
        available_size.block_size);
    layout_box_->SetOverrideContainingBlockContentLogicalHeight(
        available_size.inline_size);
  }
  // TODO(layout-ng): Does this handle scrollbars correctly?
  if (constraint_space.IsFixedSizeInline()) {
    layout_box_->SetOverrideLogicalContentWidth(
        constraint_space.AvailableSize().inline_size -
        layout_box_->BorderAndPaddingLogicalWidth());
  }
  if (constraint_space.IsFixedSizeBlock()) {
    layout_box_->SetOverrideLogicalContentHeight(
        constraint_space.AvailableSize().block_size -
        layout_box_->BorderAndPaddingLogicalHeight());
  }

  if (layout_box_->IsLayoutNGBlockFlow() && layout_box_->NeedsLayout()) {
    ToLayoutNGBlockFlow(layout_box_)->LayoutBlockFlow::UpdateBlockLayout(true);
  } else {
    layout_box_->ForceLayout();
  }
  NGLogicalSize box_size(layout_box_->LogicalWidth(),
                         layout_box_->LogicalHeight());
  NGPhysicalSize overflow_physical_size(
      layout_box_->LayoutOverflowRect().Width(),
      layout_box_->LayoutOverflowRect().Height());
  NGLogicalSize overflow_size =
      overflow_physical_size.ConvertToLogical(writing_mode);
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, this);
  builder.SetSize(box_size)
      .SetDirection(layout_box_->StyleRef().Direction())
      .SetWritingMode(writing_mode)
      .SetOverflowSize(overflow_size);
  return builder.ToBoxFragment();
}

void NGBlockNode::UseOldOutOfFlowPositioning() {
  DCHECK(layout_box_->IsOutOfFlowPositioned());
  layout_box_->ContainingBlock()->InsertPositionedObject(layout_box_);
}

// Save static position for legacy AbsPos layout.
void NGBlockNode::SaveStaticOffsetForLegacy(const NGLogicalOffset& offset) {
  DCHECK(layout_box_->IsOutOfFlowPositioned());
  DCHECK(layout_box_->Layer());
  layout_box_->Layer()->SetStaticBlockPosition(offset.block_offset);
  layout_box_->Layer()->SetStaticInlinePosition(offset.inline_offset);
}

}  // namespace blink
