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
#include "core/layout/ng/legacy_layout_tree_walking.h"
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
                                           const NGConstraintSpace& space,
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

void UpdateLegacyMultiColumnFlowThread(
    LayoutBox* layout_box,
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& fragment) {
  LayoutBlockFlow* multicol = ToLayoutBlockFlow(layout_box);
  LayoutMultiColumnFlowThread* flow_thread = multicol->MultiColumnFlowThread();
  if (!flow_thread)
    return;

  NGWritingMode writing_mode = constraint_space.WritingMode();
  LayoutUnit column_inline_size;
  LayoutUnit flow_end;

  // Stitch the columns together.
  for (const RefPtr<NGPhysicalFragment> child : fragment.Children()) {
    NGFragment child_fragment(writing_mode, *child);
    flow_end += child_fragment.BlockSize();
    column_inline_size = child_fragment.InlineSize();
  }

  if (LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet()) {
    NGFragment logical_fragment(writing_mode, fragment);
    column_set->SetLogicalWidth(logical_fragment.InlineSize());
    column_set->SetLogicalHeight(logical_fragment.BlockSize());
    column_set->EndFlow(flow_end);
    column_set->UpdateFromNG();
  }
  // TODO(mstensho): Update all column boxes, not just the first column set
  // (like we do above). This is needed to support column-span:all.
  for (LayoutBox* column_box = flow_thread->FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox())
    column_box->ClearNeedsLayout();

  flow_thread->ValidateColumnSets();
  flow_thread->SetLogicalWidth(column_inline_size);
  flow_thread->SetLogicalHeight(flow_end);
  flow_thread->ClearNeedsLayout();
}

// Return the total amount of block space spent on a node by fragments
// preceding this one (but not including this one).
LayoutUnit PreviouslyUsedBlockSpace(const NGConstraintSpace& constraint_space,
                                    const NGPhysicalBoxFragment& fragment) {
  if (!constraint_space.HasBlockFragmentation())
    return LayoutUnit();
  const auto* break_token = ToNGBlockBreakToken(fragment.BreakToken());
  if (!break_token)
    return LayoutUnit();
  NGBoxFragment logical_fragment(constraint_space.WritingMode(), fragment);
  return break_token->UsedBlockSize() - logical_fragment.BlockSize();
}

// Return true if the specified fragment is the first generated fragment of
// some node.
bool IsFirstFragment(const NGConstraintSpace& constraint_space,
                     const NGPhysicalBoxFragment& fragment) {
  return PreviouslyUsedBlockSpace(constraint_space, fragment) <= LayoutUnit();
}

// Return true if the specified fragment is the final fragment of some node.
bool IsLastFragment(const NGPhysicalBoxFragment& fragment) {
  const auto* break_token = fragment.BreakToken();
  return !break_token || break_token->IsFinished();
}

}  // namespace

NGBlockNode::NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

RefPtr<NGLayoutResult> NGBlockNode::Layout(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    return RunOldLayout(constraint_space);
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
    CopyFragmentDataToLayoutBox(constraint_space, *layout_result);

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
  NGBlockLayoutAlgorithm minmax_algorithm(*this, *constraint_space);
  Optional<MinMaxSize> maybe_sizes = minmax_algorithm.ComputeMinMaxSize();
  if (maybe_sizes.has_value())
    return *maybe_sizes;

  // Have to synthesize this value.
  RefPtr<NGLayoutResult> layout_result = Layout(*constraint_space);
  NGBoxFragment min_fragment(
      FromPlatformWritingMode(Style().GetWritingMode()),
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
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

  layout_result = Layout(*constraint_space);
  NGBoxFragment max_fragment(
      FromPlatformWritingMode(Style().GetWritingMode()),
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
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
  auto* block = ToLayoutNGBlockFlow(box_);
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (AreNGBlockFlowChildrenInline(block))
    return NGInlineNode(block);
  return NGBlockNode(ToLayoutBox(child));
}

bool NGBlockNode::CanUseNewLayout() const {
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
    const NGLayoutResult& layout_result) {
  DCHECK(layout_result.PhysicalFragment());
  const NGPhysicalBoxFragment& physical_fragment =
      ToNGPhysicalBoxFragment(*layout_result.PhysicalFragment());

  if (box_->Style()->SpecifiesColumns()) {
    UpdateLegacyMultiColumnFlowThread(box_, constraint_space,
                                      physical_fragment);
  }
  NGBoxFragment fragment(constraint_space.WritingMode(), physical_fragment);
  // For each fragment we process, we'll accumulate the logical height and
  // logical intrinsic content box height. We reset it at the first fragment,
  // and accumulate at each method call for fragments belonging to the same
  // layout object. Logical width will only be set at the first fragment and is
  // expected to remain the same throughout all subsequent fragments, since
  // legacy layout doesn't support non-uniform fragmentainer widths.
  LayoutUnit logical_height;
  LayoutUnit intrinsic_content_logical_height;
  if (IsFirstFragment(constraint_space, physical_fragment)) {
    box_->SetLogicalWidth(fragment.InlineSize());
  } else {
    DCHECK_EQ(box_->LogicalWidth(), fragment.InlineSize())
        << "Variable fragment inline size not supported";
    logical_height = box_->LogicalHeight();
    intrinsic_content_logical_height = box_->IntrinsicContentLogicalHeight();
  }
  logical_height += fragment.BlockSize();
  intrinsic_content_logical_height += fragment.OverflowSize().block_size;
  NGBoxStrut border_scrollbar_padding =
      ComputeBorders(constraint_space, Style()) +
      ComputePadding(constraint_space, Style()) + GetScrollbarSizes(box_);
  if (IsLastFragment(physical_fragment))
    intrinsic_content_logical_height -= border_scrollbar_padding.BlockSum();
  box_->SetLogicalHeight(logical_height);
  box_->SetIntrinsicContentLogicalHeight(intrinsic_content_logical_height);

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

  for (const auto& child_fragment : physical_fragment.Children()) {
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
      const auto& box_fragment = *ToNGPhysicalBoxFragment(child_fragment.Get());
      if (IsFirstFragment(constraint_space, box_fragment))
        CopyChildFragmentPosition(box_fragment);

      if (child_fragment->GetLayoutObject()->IsLayoutBlockFlow())
        ToLayoutBlockFlow(child_fragment->GetLayoutObject())
            ->AddOverflowFromFloats();
    }
  }

  if (box_->IsLayoutBlock() && IsLastFragment(physical_fragment)) {
    LayoutBlock* block = ToLayoutBlock(box_);
    NGWritingMode writing_mode = constraint_space.WritingMode();
    NGBoxFragment fragment(writing_mode, physical_fragment);
    LayoutUnit overflow_size = fragment.OverflowSize().block_size;
    overflow_size +=
        PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    block->LayoutPositionedObjects(true);
    block->ComputeOverflow(overflow_size - border_scrollbar_padding.block_end);
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
