// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutBlock.h"
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
#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_min_max_content_size.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

namespace {

RefPtr<NGLayoutResult> LayoutWithAlgorithm(const ComputedStyle& style,
                                           NGBlockNode node,
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
    layout_box->SetX(container_width - fragment.Offset().left -
                     fragment.Size().width);
  } else {
    layout_box->SetX(fragment.Offset().left);
  }
  layout_box->SetY(fragment.Offset().top);
}

// Similar to FragmentPositionUpdated but for floats.
// - Updates layout object's geometric information.
// - Creates legacy FloatingObject and attached it to the provided parent.
void FloatingObjectPositionedUpdated(const NGPositionedFloat& positioned_float,
                                     LayoutBox* parent) {
  NGPhysicalBoxFragment* box_fragment = positioned_float.fragment.Get();
  FragmentPositionUpdated(*box_fragment);

  LayoutBox* layout_box = ToLayoutBox(box_fragment->GetLayoutObject());
  DCHECK(layout_box->IsFloating());

  if (parent && parent->IsLayoutBlockFlow()) {
    LayoutBlockFlow& containing_block = *ToLayoutBlockFlow(parent);
    FloatingObject* floating_object =
        containing_block.InsertFloatingObject(*layout_box);
    floating_object->SetIsInPlacedTree(false);
    LayoutUnit logical_left = positioned_float.paint_offset.inline_offset;
    LayoutUnit logical_top = positioned_float.paint_offset.block_offset;
    // Update floating_object's logical left and top position (which is the same
    // as inline and block offset). Note that this does not update the actual
    // LayoutObject established by the float, just the FloatingObject.
    containing_block.SetLogicalLeftForFloat(*floating_object, logical_left);
    containing_block.SetLogicalTopForFloat(*floating_object, logical_top);
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
    column_set->SetWidth(fragment->Size().width);
    column_set->SetHeight(fragment->Size().height);

    // TODO(mstensho): This value has next to nothing to do with the flow thread
    // portion size, but at least it's usually better than zero.
    column_set->EndFlow(fragment->Size().height);

    column_set->ClearNeedsLayout();
  }
  // TODO(mstensho): Fix the relatively nonsensical values here (the content box
  // size of the multicol container has very little to do with the price of
  // eggs).
  flow_thread->SetWidth(fragment->Size().width);
  flow_thread->SetHeight(fragment->Size().height);

  flow_thread->ValidateColumnSets();
  flow_thread->ClearNeedsLayout();
}

}  // namespace

NGBlockNode::NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box) {
  if (box)
    box->SetLayoutNGInline(false);
}

RefPtr<NGLayoutResult> NGBlockNode::Layout(NGConstraintSpace* constraint_space,
                                           NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    return RunOldLayout(*constraint_space);
  }

  RefPtr<NGLayoutResult> layout_result =
      LayoutWithAlgorithm(Style(), *this, constraint_space, break_token);

  CopyFragmentDataToLayoutBox(*constraint_space, layout_result.Get());
  return layout_result;
}

MinMaxContentSize NGBlockNode::ComputeMinMaxContentSize() {
  MinMaxContentSize sizes;
  if (!CanUseNewLayout()) {
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    LayoutUnit border_and_padding = box_->BorderAndPaddingLogicalWidth();
    sizes.min_content = box_->ComputeLogicalWidthUsing(
                            kMainOrPreferredSize, Length(kMinContent),
                            LayoutUnit(), box_->ContainingBlock()) -
                        border_and_padding;
    sizes.max_content = box_->ComputeLogicalWidthUsing(
                            kMainOrPreferredSize, Length(kMaxContent),
                            LayoutUnit(), box_->ContainingBlock()) -
                        border_and_padding;
    return sizes;
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().GetWritingMode()))
          .SetTextDirection(Style().Direction())
          .ToConstraintSpace(FromPlatformWritingMode(Style().GetWritingMode()));

  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGBlockLayoutAlgorithm minmax_algorithm(*this, constraint_space.Get());
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

NGLayoutInputNode NGBlockNode::NextSibling() const {
  LayoutObject* next_sibling = box_->NextSibling();
  if (next_sibling) {
    DCHECK(!next_sibling->IsInline());
    return NGBlockNode(ToLayoutBox(next_sibling));
  }
  return nullptr;
}

NGLayoutInputNode NGBlockNode::FirstChild() {
  LayoutObject* child = box_->SlowFirstChild();
  if (child) {
    if (box_->ChildrenInline()) {
      LayoutNGBlockFlow* block_flow = ToLayoutNGBlockFlow(GetLayoutObject());
      return NGInlineNode(block_flow);
    } else {
      return NGBlockNode(ToLayoutBox(child));
    }
  }

  return nullptr;
}

bool NGBlockNode::CanUseNewLayout() const {
  // [Multicol]: for the 1st phase of LayoutNG's multicol implementation we want
  // to utilize the existing ColumnBalancer class. That's why a multicol block
  // should be processed by Legacy Layout engine.
  if (Style().SpecifiesColumns())
    return false;

  if (!box_->IsLayoutNGBlockFlow())
    return false;

  return RuntimeEnabledFeatures::LayoutNGEnabled();
}

String NGBlockNode::ToString() const {
  return String::Format("NGBlockNode: '%s'",
                        GetLayoutObject()->DebugName().Ascii().data());
}

void NGBlockNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    NGLayoutResult* layout_result) {
  NGPhysicalBoxFragment* physical_fragment =
      ToNGPhysicalBoxFragment(layout_result->PhysicalFragment().Get());
  if (box_->Style()->SpecifiesColumns())
    UpdateLegacyMultiColumnFlowThread(box_, physical_fragment);
  box_->SetWidth(physical_fragment->Size().width);
  box_->SetHeight(physical_fragment->Size().height);
  NGBoxStrut border_scrollbar_padding =
      ComputeBorders(constraint_space, Style()) +
      ComputePadding(constraint_space, Style()) + GetScrollbarSizes(box_);
  LayoutUnit intrinsic_logical_height =
      box_->Style()->IsHorizontalWritingMode()
          ? physical_fragment->OverflowSize().height
          : physical_fragment->OverflowSize().width;
  intrinsic_logical_height -= border_scrollbar_padding.BlockSum();
  box_->SetIntrinsicContentLogicalHeight(intrinsic_logical_height);

  // TODO(ikilpatrick) is this the right thing to do?
  if (box_->IsLayoutBlockFlow()) {
    ToLayoutBlockFlow(box_)->RemoveFloatingObjects();
  }
  for (const NGPositionedFloat& positioned_float :
       physical_fragment->PositionedFloats())
    FloatingObjectPositionedUpdated(positioned_float, box_);

  for (const auto& child_fragment : physical_fragment->Children()) {
    if (child_fragment->IsPlaced())
      FragmentPositionUpdated(ToNGPhysicalBoxFragment(*child_fragment));

    for (const NGPositionedFloat& positioned_float :
         ToNGPhysicalBoxFragment(child_fragment.Get())->PositionedFloats()) {
      FloatingObjectPositionedUpdated(
          positioned_float, ToLayoutBox(child_fragment->GetLayoutObject()));
    }
  }

  if (box_->IsLayoutBlock()) {
    ToLayoutBlock(box_)->LayoutPositionedObjects(true);
    NGWritingMode writing_mode =
        FromPlatformWritingMode(Style().GetWritingMode());
    NGBoxFragment fragment(writing_mode, physical_fragment);
    ToLayoutBlock(box_)->ComputeOverflow(fragment.OverflowSize().block_size -
                                         border_scrollbar_padding.block_end);
  }

  box_->UpdateAfterLayout();
  box_->ClearNeedsLayout();

  if (box_->IsLayoutBlockFlow()) {
    ToLayoutBlockFlow(box_)->UpdateIsSelfCollapsing();
  }
}

RefPtr<NGLayoutResult> NGBlockNode::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  NGLogicalSize available_size = constraint_space.PercentageResolutionSize();
  LayoutObject* containing_block = box_->ContainingBlock();
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
    box_->SetOverrideContainingBlockContentLogicalWidth(
        available_size.inline_size);
    box_->SetOverrideContainingBlockContentLogicalHeight(
        available_size.block_size);
  } else {
    // OverrideContainingBlock should be in containing block writing mode.
    box_->SetOverrideContainingBlockContentLogicalWidth(
        available_size.block_size);
    box_->SetOverrideContainingBlockContentLogicalHeight(
        available_size.inline_size);
  }
  // TODO(layout-ng): Does this handle scrollbars correctly?
  if (constraint_space.IsFixedSizeInline()) {
    box_->SetOverrideLogicalContentWidth(
        constraint_space.AvailableSize().inline_size -
        box_->BorderAndPaddingLogicalWidth());
  }
  if (constraint_space.IsFixedSizeBlock()) {
    box_->SetOverrideLogicalContentHeight(
        constraint_space.AvailableSize().block_size -
        box_->BorderAndPaddingLogicalHeight());
  }

  if (box_->IsLayoutNGBlockFlow() && box_->NeedsLayout()) {
    ToLayoutNGBlockFlow(box_)->LayoutBlockFlow::UpdateBlockLayout(true);
  } else {
    box_->ForceLayout();
  }
  NGLogicalSize box_size(box_->LogicalWidth(), box_->LogicalHeight());
  NGPhysicalSize overflow_physical_size(box_->LayoutOverflowRect().Width(),
                                        box_->LayoutOverflowRect().Height());
  NGLogicalSize overflow_size =
      overflow_physical_size.ConvertToLogical(writing_mode);
  NGFragmentBuilder builder(NGPhysicalFragment::kFragmentBox, *this);
  builder.SetSize(box_size)
      .SetDirection(box_->StyleRef().Direction())
      .SetWritingMode(writing_mode)
      .SetOverflowSize(overflow_size);
  return builder.ToBoxFragment();
}

void NGBlockNode::UseOldOutOfFlowPositioning() {
  DCHECK(box_->IsOutOfFlowPositioned());
  box_->ContainingBlock()->InsertPositionedObject(box_);
}

// Save static position for legacy AbsPos layout.
void NGBlockNode::SaveStaticOffsetForLegacy(const NGLogicalOffset& offset) {
  DCHECK(box_->IsOutOfFlowPositioned());
  DCHECK(box_->Layer());
  box_->Layer()->SetStaticBlockPosition(offset.block_offset);
  box_->Layer()->SetStaticInlinePosition(offset.inline_offset);
}

}  // namespace blink
