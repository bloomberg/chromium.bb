// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_node.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutMultiColumnFlowThread.h"
#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/LayoutTable.h"
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
#include "core/layout/ng/ng_fragmentation_utils.h"
#include "core/layout/ng/ng_layout_input_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_page_layout_algorithm.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "core/paint/PaintLayer.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline LayoutMultiColumnFlowThread* GetFlowThread(const LayoutBox& box) {
  if (!box.IsLayoutBlockFlow())
    return nullptr;
  return ToLayoutBlockFlow(box).MultiColumnFlowThread();
}

scoped_refptr<NGLayoutResult> LayoutWithAlgorithm(
    const ComputedStyle& style,
    NGBlockNode node,
    LayoutBox* box,
    const NGConstraintSpace& space,
    NGBreakToken* break_token) {
  auto* token = ToNGBlockBreakToken(break_token);
  // If there's a legacy layout box, we can only do block fragmentation if we
  // would have done block fragmentation with the legacy engine. Otherwise
  // writing data back into the legacy tree will fail. Look for the flow
  // thread.
  if (!box || GetFlowThread(*box)) {
    if (style.IsOverflowPaged())
      return NGPageLayoutAlgorithm(node, space, token).Layout();
    if (style.SpecifiesColumns())
      return NGColumnLayoutAlgorithm(node, space, token).Layout();
    NOTREACHED();
  }
  return NGBlockLayoutAlgorithm(node, space, token).Layout();
}

bool IsFloatFragment(const NGPhysicalFragment& fragment) {
  const LayoutObject* layout_object = fragment.GetLayoutObject();
  return layout_object && layout_object->IsFloating() && fragment.IsBox();
}

void UpdateLegacyMultiColumnFlowThread(
    NGBlockNode node,
    LayoutMultiColumnFlowThread* flow_thread,
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& fragment) {
  NGWritingMode writing_mode = constraint_space.WritingMode();
  LayoutUnit flow_end;
  LayoutUnit column_block_size;
  bool has_processed_first_child = false;

  // Stitch the columns together.
  for (const scoped_refptr<NGPhysicalFragment> child : fragment.Children()) {
    NGFragment child_fragment(writing_mode, *child);
    flow_end += child_fragment.BlockSize();
    // Non-uniform fragmentainer widths not supported by legacy layout.
    DCHECK(!has_processed_first_child ||
           flow_thread->LogicalWidth() == child_fragment.InlineSize());
    if (!has_processed_first_child) {
      // The offset of the flow thread should be the same as that of the first
      // first column.
      flow_thread->SetX(child->Offset().left);
      flow_thread->SetY(child->Offset().top);
      flow_thread->SetLogicalWidth(child_fragment.InlineSize());
      column_block_size = child_fragment.BlockSize();
      has_processed_first_child = true;
    }
  }

  if (LayoutMultiColumnSet* column_set = flow_thread->FirstMultiColumnSet()) {
    NGFragment logical_fragment(writing_mode, fragment);
    auto border_scrollbar_padding =
        CalculateBorderScrollbarPadding(constraint_space, node.Style(), node);

    column_set->SetLogicalLeft(border_scrollbar_padding.inline_start);
    column_set->SetLogicalTop(border_scrollbar_padding.block_start);
    column_set->SetLogicalWidth(logical_fragment.InlineSize() -
                                border_scrollbar_padding.InlineSum());
    column_set->SetLogicalHeight(column_block_size);
    column_set->EndFlow(flow_end);
    column_set->UpdateFromNG();
  }
  // TODO(mstensho): Update all column boxes, not just the first column set
  // (like we do above). This is needed to support column-span:all.
  for (LayoutBox* column_box = flow_thread->FirstMultiColumnBox(); column_box;
       column_box = column_box->NextSiblingMultiColumnBox()) {
    column_box->ClearNeedsLayout();
    column_box->UpdateAfterLayout();
  }

  flow_thread->ValidateColumnSets();
  flow_thread->SetLogicalHeight(flow_end);
  flow_thread->UpdateAfterLayout();
  flow_thread->ClearNeedsLayout();
}

}  // namespace

NGBlockNode::NGBlockNode(LayoutBox* box) : NGLayoutInputNode(box, kBlock) {}

scoped_refptr<NGLayoutResult> NGBlockNode::Layout(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token) {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout()) {
    return RunOldLayout(constraint_space);
  }
  scoped_refptr<NGLayoutResult> layout_result;
  if (box_->IsLayoutNGBlockFlow()) {
    layout_result = ToLayoutNGBlockFlow(box_)->CachedLayoutResult(
        constraint_space, break_token);
    if (layout_result)
      return layout_result;
  }

  layout_result =
      LayoutWithAlgorithm(Style(), *this, box_, constraint_space, break_token);
  LayoutNGBlockFlow* block_flow =
      box_->IsLayoutNGBlockFlow() ? ToLayoutNGBlockFlow(box_) : nullptr;
  if (block_flow) {
    block_flow->SetCachedLayoutResult(constraint_space, break_token,
                                      layout_result);
  }

  if (layout_result->Status() == NGLayoutResult::kSuccess &&
      layout_result->UnpositionedFloats().IsEmpty()) {
    DCHECK(layout_result->PhysicalFragment());

    // If this node has inline children, enable LayoutNGPaintFragmets.
    if (block_flow && FirstChild().IsInline()) {
      block_flow->SetPaintFragment(layout_result->PhysicalFragment());
    }

    // TODO(kojii): Even when we paint fragments, there seem to be some data we
    // need to copy to LayoutBox. Review if we can minimize the copy.
    CopyFragmentDataToLayoutBox(constraint_space, *layout_result);
  }

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

  scoped_refptr<NGConstraintSpace> constraint_space =
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
  scoped_refptr<NGLayoutResult> layout_result = Layout(*constraint_space);
  NGBoxFragment min_fragment(
      FromPlatformWritingMode(Style().GetWritingMode()),
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
  sizes.min_size = min_fragment.Size().inline_size;

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
  sizes.max_size = max_fragment.Size().inline_size;
  return sizes;
}

NGBoxStrut NGBlockNode::GetScrollbarSizes() const {
  NGPhysicalBoxStrut sizes;
  const ComputedStyle* style = GetLayoutObject()->Style();
  if (!style->IsOverflowVisible()) {
    const LayoutBox* box = ToLayoutBox(GetLayoutObject());
    LayoutUnit vertical = LayoutUnit(box->VerticalScrollbarWidth());
    LayoutUnit horizontal = LayoutUnit(box->HorizontalScrollbarHeight());
    sizes.bottom = horizontal;
    if (style->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
      sizes.left = vertical;
    else
      sizes.right = vertical;
  }
  return sizes.ConvertToLogical(
      FromPlatformWritingMode(style->GetWritingMode()), style->Direction());
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
  if (AreNGBlockFlowChildrenInline(block)) {
    // TODO(kojii): Invalidate PrepareLayout() more efficiently.
    NGInlineNode node(block);
    node.InvalidatePrepareLayout();
    node.PrepareLayout();
    return node;
  }
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
  intrinsic_content_logical_height += layout_result.IntrinsicBlockSize();
  NGBoxStrut border_scrollbar_padding =
      ComputeBorders(constraint_space, Style()) +
      ComputePadding(constraint_space, Style()) + GetScrollbarSizes();
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
    box_->SetMargin(ComputePhysicalMargins(constraint_space, Style()));
  }

  LayoutMultiColumnFlowThread* flow_thread = GetFlowThread(*box_);
  if (flow_thread) {
    PlaceChildrenInFlowThread(constraint_space, physical_fragment);
  } else {
    NGPhysicalOffset offset_from_start;
    if (constraint_space.HasBlockFragmentation()) {
      // Need to include any block space that this container has used in
      // previous fragmentainers. The offset of children will be relative to
      // the container, in flow thread coordinates, i.e. the model where
      // everything is represented as one single strip, rather than being
      // sliced and translated into columns.

      // TODO(mstensho): writing modes
      offset_from_start.top =
          PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    }
    PlaceChildrenInLayoutBox(constraint_space, physical_fragment,
                             offset_from_start);
  }

  if (box_->IsLayoutBlock() && IsLastFragment(physical_fragment)) {
    LayoutBlock* block = ToLayoutBlock(box_);
    NGWritingMode writing_mode = constraint_space.WritingMode();
    NGBoxFragment fragment(writing_mode, physical_fragment);
    LayoutUnit intrinsic_block_size = layout_result.IntrinsicBlockSize();
    if (constraint_space.HasBlockFragmentation()) {
      intrinsic_block_size +=
          PreviouslyUsedBlockSpace(constraint_space, physical_fragment);
    }
    block->LayoutPositionedObjects(/* relayout_children */ false);

    if (flow_thread) {
      UpdateLegacyMultiColumnFlowThread(*this, flow_thread, constraint_space,
                                        physical_fragment);
    }

    // |ComputeOverflow()| below calls |AddOverflowFromChildren()|, which
    // computes visual overflow from |RootInlineBox| if |ChildrenInline()|.
    block->ComputeOverflow(intrinsic_block_size -
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

void NGBlockNode::PlaceChildrenInLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& physical_fragment,
    const NGPhysicalOffset& offset_from_start) {
  for (const auto& child_fragment : physical_fragment.Children()) {
    auto* child_object = child_fragment->GetLayoutObject();
    DCHECK(child_fragment->IsPlaced());

    // At the moment "anonymous" fragments for inline layout will have the same
    // layout object as ourselves, we need to copy its floats across.
    if (child_object == box_) {
      for (const auto& maybe_float_fragment :
           ToNGPhysicalBoxFragment(child_fragment.get())->Children()) {
        // The child of the anonymous fragment might be just a line-box
        // fragment - ignore.
        if (IsFloatFragment(*maybe_float_fragment)) {
          // We need to include the anonymous fragments offset here for the
          // correct position.
          CopyChildFragmentPosition(
              ToNGPhysicalBoxFragment(*maybe_float_fragment),
              offset_from_start + child_fragment->Offset());
        }
      }
      continue;
    }
    const auto& box_fragment = *ToNGPhysicalBoxFragment(child_fragment.get());
    if (IsFirstFragment(constraint_space, box_fragment))
      CopyChildFragmentPosition(box_fragment, offset_from_start);

    if (child_object->IsLayoutBlockFlow())
      ToLayoutBlockFlow(child_object)->AddOverflowFromFloats();
  }
}

void NGBlockNode::PlaceChildrenInFlowThread(
    const NGConstraintSpace& constraint_space,
    const NGPhysicalBoxFragment& physical_fragment) {
  LayoutUnit flowthread_offset;
  for (const auto& child : physical_fragment.Children()) {
    // Each anonymous child of a multicol container constitutes one column.
    DCHECK(child->IsPlaced());
    DCHECK(child->GetLayoutObject() == box_);

    // TODO(mstensho): writing modes
    NGPhysicalOffset offset(LayoutUnit(), flowthread_offset);

    // Position each child node in the first column that they occur, relatively
    // to the block-start of the flow thread.
    const auto* column = ToNGPhysicalBoxFragment(child.get());
    PlaceChildrenInLayoutBox(constraint_space, *column, offset);
    const auto* token = ToNGBlockBreakToken(column->BreakToken());
    flowthread_offset = token->UsedBlockSize();
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
  // The flow thread, however, is invisible to LayoutNG, so we need to make
  // an exception there.
  DCHECK(box_ == layout_box->ContainingBlock() ||
         (layout_box->ContainingBlock()->IsLayoutFlowThread() &&
          box_ == layout_box->ContainingBlock()->ContainingBlock()));

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
    floating_object->SetX(fragment.Offset().left + additional_offset.left -
                          layout_box->MarginLeft());
    floating_object->SetY(fragment.Offset().top + additional_offset.top -
                          layout_box->MarginTop());
    floating_object->SetIsPlaced(true);
    floating_object->SetIsInPlacedTree(true);
  }
}

scoped_refptr<NGLayoutResult> NGBlockNode::LayoutAtomicInline(
    const NGConstraintSpace& parent_constraint_space,
    bool use_first_line_style) {
  NGConstraintSpaceBuilder space_builder(parent_constraint_space);
  space_builder.SetUseFirstLineStyle(use_first_line_style);

  // Request to compute baseline during the layout, except when we know the box
  // would synthesize box-baseline.
  if (NGBaseline::ShouldPropagateBaselines(ToLayoutBox(GetLayoutObject()))) {
    space_builder.AddBaselineRequest(
        {NGBaselineAlgorithmType::kAtomicInline,
         IsHorizontalWritingMode(parent_constraint_space.WritingMode())
             ? FontBaseline::kAlphabeticBaseline
             : FontBaseline::kIdeographicBaseline});
  }

  const ComputedStyle& style = Style();
  scoped_refptr<NGConstraintSpace> constraint_space =
      space_builder.SetIsNewFormattingContext(true)
          .SetIsShrinkToFit(true)
          .SetAvailableSize(parent_constraint_space.AvailableSize())
          .SetPercentageResolutionSize(
              parent_constraint_space.PercentageResolutionSize())
          .SetTextDirection(style.Direction())
          .ToConstraintSpace(FromPlatformWritingMode(style.GetWritingMode()));
  return Layout(*constraint_space);
}

scoped_refptr<NGLayoutResult> NGBlockNode::RunOldLayout(
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
        (constraint_space.AvailableSize().inline_size -
         box_->BorderAndPaddingLogicalWidth())
            .ClampNegativeToZero());
  }
  if (constraint_space.IsFixedSizeBlock()) {
    box_->SetOverrideLogicalContentHeight(
        (constraint_space.AvailableSize().block_size -
         box_->BorderAndPaddingLogicalHeight())
            .ClampNegativeToZero());
  }

  if (box_->IsLayoutNGBlockFlow() && box_->NeedsLayout()) {
    ToLayoutNGBlockFlow(box_)->LayoutBlockFlow::UpdateBlockLayout(true);
  } else {
    box_->ForceLayout();
  }
  NGLogicalSize box_size(box_->LogicalWidth(), box_->LogicalHeight());
  // TODO(kojii): Implement use_first_line_style.
  NGFragmentBuilder builder(*this, box_->Style(), writing_mode,
                            box_->StyleRef().Direction());
  builder.SetBoxType(NGPhysicalFragment::NGBoxType::kOldLayoutRoot);
  builder.SetInlineSize(box_size.inline_size);
  builder.SetBlockSize(box_size.block_size);

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
        AddAtomicInlineBaselineFromOldLayout(
            request, constraint_space.UseFirstLineStyle(), builder);
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
  // Block-level boxes do not have atomic inline baseline.
  // This includes form controls when 'display:block' is applied.
  if (box_->IsLayoutBlock() && !box_->IsInline())
    return;

  LineDirectionMode line_direction =
      IsHorizontalWritingMode(builder->WritingMode())
          ? LineDirectionMode::kHorizontalLine
          : LineDirectionMode::kVerticalLine;
  LayoutUnit position = LayoutUnit(box_->BaselinePosition(
      request.baseline_type, is_first_line, line_direction));

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
