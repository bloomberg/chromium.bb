// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/MinMaxSize.h"
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

bool IsFloatFragment(const NGPhysicalFragment& fragment) {
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  return layout_object && layout_object->IsFloating() && fragment.IsBox();
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

    column_set->UpdateFromNG();
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

NGBlockNode::NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

RefPtr<NGLayoutResult> NGBlockNode::Layout(NGConstraintSpace* constraint_space,
                                           NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    return RunOldLayout(*constraint_space);
  }
  RefPtr<NGLayoutResult> layout_result;
  if (box_->IsLayoutNGBlockFlow()) {
    layout_result = ToLayoutNGBlockFlow(box_)->CachedLayoutResult(
        constraint_space, break_token);
    if (layout_result)
      return layout_result;
  }

  layout_result =
      LayoutWithAlgorithm(Style(), *this, constraint_space, break_token);
  if (box_->IsLayoutNGBlockFlow()) {
    ToLayoutNGBlockFlow(box_)->SetCachedLayoutResult(
        constraint_space, break_token, layout_result);
  }

  if (layout_result->Status() == NGLayoutResult::kSuccess &&
      layout_result->UnpositionedFloats().IsEmpty())
    CopyFragmentDataToLayoutBox(*constraint_space, layout_result.Get());

  return layout_result;
}

MinMaxSize NGBlockNode::ComputeMinMaxSize() {
  MinMaxSize sizes;
  if (!CanUseNewLayout()) {
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    LayoutUnit border_and_padding = box_->BorderAndPaddingLogicalWidth();
    sizes.min_size = box_->ComputeLogicalWidthUsing(
                         kMainOrPreferredSize, Length(kMinContent),
                         LayoutUnit(), box_->ContainingBlock()) -
                     border_and_padding;
    sizes.max_size = box_->ComputeLogicalWidthUsing(
                         kMainOrPreferredSize, Length(kMaxContent),
                         LayoutUnit(), box_->ContainingBlock()) -
                     border_and_padding;
    return sizes;
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().GetWritingMode()),
          /* icb_size */ {NGSizeIndefinite, NGSizeIndefinite})
          .SetTextDirection(Style().Direction())
          .ToConstraintSpace(FromPlatformWritingMode(Style().GetWritingMode()));

  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGBlockLayoutAlgorithm minmax_algorithm(*this, constraint_space.Get());
  Optional<MinMaxSize> maybe_sizes = minmax_algorithm.ComputeMinMaxSize();
  if (maybe_sizes.has_value())
    return *maybe_sizes;

  // Have to synthesize this value.
  RefPtr<NGLayoutResult> layout_result = Layout(constraint_space.Get());
  NGPhysicalFragment* physical_fragment =
      layout_result->PhysicalFragment().Get();
  NGBoxFragment min_fragment(FromPlatformWritingMode(Style().GetWritingMode()),
                             ToNGPhysicalBoxFragment(physical_fragment));
  sizes.min_size = min_fragment.OverflowSize().inline_size;

  // Now, redo with infinite space for max_content
  constraint_space =
      NGConstraintSpaceBuilder(
          FromPlatformWritingMode(Style().GetWritingMode()),
          /* icb_size */ {NGSizeIndefinite, NGSizeIndefinite})
          .SetTextDirection(Style().Direction())
          .SetAvailableSize({LayoutUnit::Max(), LayoutUnit()})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .ToConstraintSpace(FromPlatformWritingMode(Style().GetWritingMode()));

  layout_result = Layout(constraint_space.Get());
  physical_fragment = layout_result->PhysicalFragment().Get();
  NGBoxFragment max_fragment(FromPlatformWritingMode(Style().GetWritingMode()),
                             ToNGPhysicalBoxFragment(physical_fragment));
  sizes.max_size = max_fragment.OverflowSize().inline_size;
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

  // LayoutBox::Margin*() should be used value, while we set computed value
  // here. This is not entirely correct, but these values are not used for
  // layout purpose.
  // BaselinePosition() relies on margins set to the box, and computed value is
  // good enough for it to work correctly.
  // Set this only for atomic inlines, or we end up adding margins twice.
  if (box_->IsAtomicInlineLevel()) {
    NGBoxStrut margins =
        ComputeMargins(constraint_space, Style(),
                       constraint_space.WritingMode(), Style().Direction());
    box_->SetMarginBefore(margins.block_start);
    box_->SetMarginAfter(margins.block_end);
    box_->SetMarginStart(margins.inline_start);
    box_->SetMarginEnd(margins.inline_end);
  }

  for (const auto& child_fragment : physical_fragment->Children()) {
    DCHECK(child_fragment->IsPlaced());

    // At the moment "anonymous" fragments for inline layout will have the same
    // layout object as ourselves, we need to copy its floats across.
    if (child_fragment->GetLayoutObject() == box_) {
      for (const auto& maybe_float_fragment :
           ToNGPhysicalBoxFragment(child_fragment.Get())->Children()) {
        // The child of the anonymous fragment might be just a line-box
        // fragment - ignore.
        if (IsFloatFragment(*maybe_float_fragment)) {
          // We need to include the anonymous fragments offset here for the
          // correct position.
          CopyChildFragmentPosition(
              ToNGPhysicalBoxFragment(*maybe_float_fragment),
              child_fragment->Offset());
        }
      }
    } else {
      CopyChildFragmentPosition(ToNGPhysicalBoxFragment(*child_fragment));

      if (child_fragment->GetLayoutObject()->IsLayoutBlockFlow())
        ToLayoutBlockFlow(child_fragment->GetLayoutObject())
            ->AddOverflowFromFloats();
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
    LayoutBlockFlow* block_flow = ToLayoutBlockFlow(box_);
    block_flow->UpdateIsSelfCollapsing();

    if (block_flow->CreatesNewFormattingContext())
      block_flow->AddOverflowFromFloats();
  }
}

// Copies data back to the legacy layout tree for a given child fragment.
void NGBlockNode::CopyChildFragmentPosition(
    const NGPhysicalFragment& fragment,
    const NGPhysicalOffset& additional_offset) {
  LayoutBox* layout_box = ToLayoutBox(fragment.GetLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  // We should only be positioning children which are relative to ourselves.
  DCHECK_EQ(box_, layout_box->ContainingBlock());

  // LegacyLayout flips vertical-rl horizontal coordinates before paint.
  // NGLayout flips X location for LegacyLayout compatibility.
  if (box_->StyleRef().IsFlippedBlocksWritingMode()) {
    LayoutUnit container_width = box_->Size().Width();
    layout_box->SetX(container_width - fragment.Offset().left -
                     additional_offset.left - fragment.Size().width);
  } else {
    layout_box->SetX(fragment.Offset().left + additional_offset.left);
  }
  layout_box->SetY(fragment.Offset().top + additional_offset.top);

  // Floats need an associated FloatingObject for painting.
  if (IsFloatFragment(fragment) && box_->IsLayoutBlockFlow()) {
    FloatingObject* floating_object =
        ToLayoutBlockFlow(box_)->InsertFloatingObject(*layout_box);
    floating_object->SetIsInPlacedTree(false);
    floating_object->SetX(fragment.Offset().left + additional_offset.left);
    floating_object->SetY(fragment.Offset().top + additional_offset.top);
    floating_object->SetIsPlaced(true);
    floating_object->SetIsInPlacedTree(true);
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
  // TODO(kojii): Implement use_first_line_style.
  NGFragmentBuilder builder(*this, box_->Style(), writing_mode,
                            box_->StyleRef().Direction());
  builder.SetSize(box_size)
      .SetOverflowSize(overflow_size);

  // For now we copy the exclusion space straight through, this is incorrect
  // but needed as not all elements which participate in a BFC are switched
  // over to LayoutNG yet.
  // TODO(ikilpatrick): Remove this once the above isn't true.
  builder.SetExclusionSpace(
      WTF::WrapUnique(new NGExclusionSpace(constraint_space.ExclusionSpace())));

  CopyBaselinesFromOldLayout(constraint_space, &builder);
  return builder.ToBoxFragment();
}

void NGBlockNode::CopyBaselinesFromOldLayout(
    const NGConstraintSpace& constraint_space,
    NGFragmentBuilder* builder) {
  const Vector<NGBaselineRequest>& requests =
      constraint_space.BaselineRequests();
  if (requests.IsEmpty())
    return;

  for (const auto& request : requests) {
    switch (request.algorithm_type) {
      case NGBaselineAlgorithmType::kAtomicInline:
        AddAtomicInlineBaselineFromOldLayout(request, false, builder);
        break;
      case NGBaselineAlgorithmType::kAtomicInlineForFirstLine:
        AddAtomicInlineBaselineFromOldLayout(request, true, builder);
        break;
      case NGBaselineAlgorithmType::kFirstLine: {
        LayoutUnit position = box_->FirstLineBoxBaseline();
        if (position != -1) {
          builder->AddBaseline(request, position);
        }
        break;
      }
    }
  }
}

void NGBlockNode::AddAtomicInlineBaselineFromOldLayout(
    const NGBaselineRequest& request,
    bool is_first_line,
    NGFragmentBuilder* builder) {
  LineDirectionMode line_direction =
      IsHorizontalWritingMode(builder->WritingMode())
          ? LineDirectionMode::kHorizontalLine
          : LineDirectionMode::kVerticalLine;
  LayoutUnit position = LayoutUnit(box_->BaselinePosition(
      request.baseline_type, is_first_line, line_direction));

  // Some form controls return 0 for BaselinePosition() if 'display:block'.
  // Blocks without line boxes should not produce baselines.
  if (!position && !box_->IsAtomicInlineLevel() &&
      !box_->IsLayoutNGBlockFlow() &&
      box_->InlineBlockBaseline(line_direction) == -1) {
    return;
  }

  // BaselinePosition() uses margin edge for atomic inlines.
  if (box_->IsAtomicInlineLevel())
    position -= box_->MarginOver();

  builder->AddBaseline(request, position);
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
