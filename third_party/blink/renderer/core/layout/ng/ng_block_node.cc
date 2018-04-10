// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

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
  if (style.IsDisplayFlexibleBox())
    return NGFlexLayoutAlgorithm(node, space, token).Layout();
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
  WritingMode writing_mode = constraint_space.GetWritingMode();
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
        CalculateBorderScrollbarPadding(constraint_space, node);

    column_set->SetLogicalLeft(border_scrollbar_padding.inline_start);
    column_set->SetLogicalTop(border_scrollbar_padding.block_start);
    column_set->SetLogicalWidth(logical_fragment.InlineSize() -
                                border_scrollbar_padding.InlineSum());
    column_set->SetLogicalHeight(column_block_size);
    column_set->EndFlow(flow_end);
  }
  // TODO(mstensho): Update all column boxes, not just the first column set
  // (like we do above). This is needed to support column-span:all.

  flow_thread->UpdateFromNG();
  flow_thread->ValidateColumnSets();
  flow_thread->SetLogicalHeight(flow_end);
  flow_thread->UpdateAfterLayout();
  flow_thread->ClearNeedsLayout();
}

// For inline children, NG painters handles fragments directly, but there are
// some cases where we need to copy data to the LayoutObject tree. This function
// handles such cases.
void CopyFragmentDataToLayoutBoxForInlineChildren(
    const NGPhysicalContainerFragment& container,
    NGPhysicalOffset offset = {}) {
  for (const auto& child : container.Children()) {
    if (child->IsContainer()) {
      NGPhysicalOffset child_offset = offset + child->Offset();

      // Replaced elements and inline blocks need Location() set relative to
      // their block container.
      LayoutObject* layout_object = child->GetLayoutObject();
      if (layout_object && layout_object->IsBox()) {
        LayoutBox& layout_box = ToLayoutBox(*layout_object);
        layout_box.SetLocation(child_offset.ToLayoutPoint());
      }

      // The Location() of inline LayoutObject is relative to the
      // LayoutBlockFlow. If |child| is a block layout root (e.g., inline block,
      // float, etc.), it creates another inline formatting context. Do not copy
      // to its descendants in this case.
      if (!child->IsBlockLayoutRoot()) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            ToNGPhysicalContainerFragment(*child), child_offset);
      }
    }
  }
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
  LayoutBlockFlow* block_flow =
      box_->IsLayoutNGMixin() ? ToLayoutBlockFlow(box_) : nullptr;
  NGLayoutInputNode first_child = FirstChild();
  scoped_refptr<NGLayoutResult> layout_result;
  if (box_->IsLayoutNGMixin()) {
    layout_result = ToLayoutBlockFlow(box_)->CachedLayoutResult(
        constraint_space, break_token);
    if (layout_result) {
      block_flow->ClearPaintFragment();
      if (first_child && first_child.IsInline())
        block_flow->SetPaintFragment(layout_result->PhysicalFragment());
      return layout_result;
    }
  }

  layout_result =
      LayoutWithAlgorithm(Style(), *this, box_, constraint_space, break_token);
  if (block_flow) {
    block_flow->SetCachedLayoutResult(constraint_space, break_token,
                                      layout_result);
    block_flow->ClearPaintFragment();
  }

  if (layout_result->Status() == NGLayoutResult::kSuccess &&
      layout_result->UnpositionedFloats().IsEmpty()) {
    DCHECK(layout_result->PhysicalFragment());

    if (block_flow && first_child && first_child.IsInline()) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));

      block_flow->SetPaintFragment(layout_result->PhysicalFragment());
    }

    // TODO(kojii): Even when we paint fragments, there seem to be some data we
    // need to copy to LayoutBox. Review if we can minimize the copy.
    CopyFragmentDataToLayoutBox(constraint_space, *layout_result);
  }

  return layout_result;
}

MinMaxSize NGBlockNode::ComputeMinMaxSize(
    const MinMaxSizeInput& input,
    const NGConstraintSpace* constraint_space) {
  MinMaxSize sizes;
  if (!CanUseNewLayout()) {
    // TODO(layout-ng): This could be somewhat optimized by directly calling
    // computeIntrinsicLogicalWidths, but that function is currently private.
    // Consider doing that if this becomes a performance issue.
    sizes.min_size = box_->ComputeLogicalWidthUsing(
        kMainOrPreferredSize, Length(kMinContent), LayoutUnit(),
        box_->ContainingBlock());
    sizes.max_size = box_->ComputeLogicalWidthUsing(
        kMainOrPreferredSize, Length(kMaxContent), LayoutUnit(),
        box_->ContainingBlock());
    return sizes;
  }

  scoped_refptr<NGConstraintSpace> zero_constraint_space =
      NGConstraintSpaceBuilder(Style().GetWritingMode(),
                               InitialContainingBlockSize())
          .SetTextDirection(Style().Direction())
          .ToConstraintSpace(Style().GetWritingMode());

  if (!constraint_space)
    constraint_space = zero_constraint_space.get();

  // TODO(cbiesinger): For orthogonal children, we need to always synthesize.
  NGBlockLayoutAlgorithm minmax_algorithm(*this, *constraint_space);
  Optional<MinMaxSize> maybe_sizes = minmax_algorithm.ComputeMinMaxSize(input);
  if (maybe_sizes.has_value())
    return *maybe_sizes;

  // Have to synthesize this value.
  scoped_refptr<NGLayoutResult> layout_result = Layout(*zero_constraint_space);
  NGBoxFragment min_fragment(
      Style().GetWritingMode(),
      ToNGPhysicalBoxFragment(*layout_result->PhysicalFragment()));
  sizes.min_size = min_fragment.Size().inline_size;

  // Now, redo with infinite space for max_content
  scoped_refptr<NGConstraintSpace> infinite_constraint_space =
      NGConstraintSpaceBuilder(Style().GetWritingMode(),
                               InitialContainingBlockSize())
          .SetTextDirection(Style().Direction())
          .SetAvailableSize({LayoutUnit::Max(), LayoutUnit()})
          .SetPercentageResolutionSize({LayoutUnit(), LayoutUnit()})
          .ToConstraintSpace(Style().GetWritingMode());

  layout_result = Layout(*infinite_constraint_space);
  NGBoxFragment max_fragment(
      Style().GetWritingMode(),
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
    if (box->ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
      sizes.left = vertical;
    else
      sizes.right = vertical;
  }
  return sizes.ConvertToLogical(style->GetWritingMode(), style->Direction());
}

NGLayoutInputNode NGBlockNode::NextSibling() const {
  LayoutObject* next_sibling = box_->NextSibling();
  if (next_sibling) {
    DCHECK(!next_sibling->IsInline());
    return NGBlockNode(ToLayoutBox(next_sibling));
  }
  return nullptr;
}

NGLayoutInputNode NGBlockNode::FirstChild() const {
  auto* block = ToLayoutBlockFlow(box_);
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (AreNGBlockFlowChildrenInline(block))
    return NGInlineNode(block);
  return NGBlockNode(ToLayoutBox(child));
}

bool NGBlockNode::CanUseNewLayout(const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());

  // When the style has |ForceLegacyLayout|, it's usually not LayoutNGMixin,
  // but anonymous block can be.
  return box.IsLayoutNGMixin() && !box.StyleRef().ForceLegacyLayout();
}

bool NGBlockNode::CanUseNewLayout() const {
  return CanUseNewLayout(*box_);
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

  NGBoxFragment fragment(constraint_space.GetWritingMode(), physical_fragment);
  NGLogicalSize fragment_logical_size = fragment.Size();
  // For each fragment we process, we'll accumulate the logical height and
  // logical intrinsic content box height. We reset it at the first fragment,
  // and accumulate at each method call for fragments belonging to the same
  // layout object. Logical width will only be set at the first fragment and is
  // expected to remain the same throughout all subsequent fragments, since
  // legacy layout doesn't support non-uniform fragmentainer widths.
  LayoutUnit logical_height;
  LayoutUnit intrinsic_content_logical_height;
  if (IsFirstFragment(constraint_space, physical_fragment)) {
    box_->SetLogicalWidth(fragment_logical_size.inline_size);
  } else {
    DCHECK_EQ(box_->LogicalWidth(), fragment_logical_size.inline_size)
        << "Variable fragment inline size not supported";
    logical_height = box_->LogicalHeight();
    intrinsic_content_logical_height = box_->IntrinsicContentLogicalHeight();
  }
  logical_height += fragment_logical_size.block_size;
  intrinsic_content_logical_height += layout_result.IntrinsicBlockSize();
  NGBoxStrut border_scrollbar_padding =
      ComputeBorders(constraint_space, Style()) +
      ComputePadding(constraint_space, Style()) + GetScrollbarSizes();
  if (IsLastFragment(physical_fragment))
    intrinsic_content_logical_height -= border_scrollbar_padding.BlockSum();
  box_->SetLogicalHeight(logical_height);
  box_->SetIntrinsicContentLogicalHeight(intrinsic_content_logical_height);
  box_->SetMargin(ComputePhysicalMargins(constraint_space, Style()));

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

    // Skip any line-boxes we have as children, this is handled within
    // NGInlineNode at the moment.
    if (!child_fragment->IsBox())
      continue;

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

  // The containing block of |layout_box| on the legacy layout side is normally
  // |box_|, but this is not an invariant. Among other things, it does not apply
  // to list item markers and multicol container children. Multicol containiner
  // children typically have their flow thread (not the multicol container
  // itself) as their containing block, and we need to use the right containing
  // block for inserting floats, flipping for writing modes, etc.
  LayoutBlock* containing_block = layout_box->ContainingBlock();

  // LegacyLayout flips vertical-rl horizontal coordinates before paint.
  // NGLayout flips X location for LegacyLayout compatibility.
  if (containing_block->StyleRef().IsFlippedBlocksWritingMode()) {
    LayoutUnit container_width = containing_block->Size().Width();
    layout_box->SetX(container_width - fragment.Offset().left -
                     additional_offset.left - fragment.Size().width);
  } else {
    layout_box->SetX(fragment.Offset().left + additional_offset.left);
  }
  layout_box->SetY(fragment.Offset().top + additional_offset.top);

  // Floats need an associated FloatingObject for painting.
  if (IsFloatFragment(fragment) && containing_block->IsLayoutBlockFlow()) {
    FloatingObject* floating_object =
        ToLayoutBlockFlow(containing_block)->InsertFloatingObject(*layout_box);
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
         IsHorizontalWritingMode(parent_constraint_space.GetWritingMode())
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
          .ToConstraintSpace(style.GetWritingMode());
  return Layout(*constraint_space);
}

scoped_refptr<NGLayoutResult> NGBlockNode::RunOldLayout(
    const NGConstraintSpace& constraint_space) {
  LayoutUnit inline_size =
      Style().LogicalWidth().IsPercent()
          ? constraint_space.PercentageResolutionSize().inline_size
          : constraint_space.AvailableSize().inline_size;
  LayoutUnit block_size =
      Style().LogicalHeight().IsPercent()
          ? constraint_space.PercentageResolutionSize().block_size
          : constraint_space.AvailableSize().block_size;
  LayoutObject* containing_block = box_->ContainingBlock();
  WritingMode writing_mode = Style().GetWritingMode();
  bool parallel_writing_mode;
  if (!containing_block) {
    parallel_writing_mode = true;
  } else {
    parallel_writing_mode = IsParallelWritingMode(
        containing_block->StyleRef().GetWritingMode(), writing_mode);
  }
  if (parallel_writing_mode) {
    box_->SetOverrideContainingBlockContentLogicalWidth(inline_size);
    box_->SetOverrideContainingBlockContentLogicalHeight(block_size);
  } else {
    // OverrideContainingBlock should be in containing block writing mode.
    box_->SetOverrideContainingBlockContentLogicalWidth(block_size);
    box_->SetOverrideContainingBlockContentLogicalHeight(inline_size);
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

  if (box_->IsLayoutNGMixin() && box_->NeedsLayout()) {
    ToLayoutBlockFlow(box_)->LayoutBlockFlow::UpdateBlockLayout(true);
  } else {
    box_->ForceLayout();
  }
  NGLogicalSize box_size(box_->LogicalWidth(), box_->LogicalHeight());
  // TODO(kojii): Implement use_first_line_style.
  NGFragmentBuilder builder(*this, box_->Style(), writing_mode,
                            box_->StyleRef().Direction());
  builder.SetIsOldLayoutRoot();
  builder.SetInlineSize(box_size.inline_size);
  builder.SetBlockSize(box_size.block_size);
  builder.SetPadding(ComputePadding(constraint_space, box_->StyleRef()));

  // For now we copy the exclusion space straight through, this is incorrect
  // but needed as not all elements which participate in a BFC are switched
  // over to LayoutNG yet.
  // TODO(ikilpatrick): Remove this once the above isn't true.
  builder.SetExclusionSpace(
      std::make_unique<NGExclusionSpace>(constraint_space.ExclusionSpace()));

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
      IsHorizontalWritingMode(builder->GetWritingMode())
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
void NGBlockNode::SaveStaticOffsetForLegacy(
    const NGLogicalOffset& offset,
    const LayoutObject* offset_container) {
  DCHECK(box_->IsOutOfFlowPositioned());
  // Only set static position if the current offset container
  // is one that Legacy layout expects static offset from.
  const LayoutObject* parent = box_->Parent();
  if (parent == offset_container ||
      (parent && parent->IsLayoutInline() &&
       parent->ContainingBlock() == offset_container)) {
    DCHECK(box_->Layer());
    box_->Layer()->SetStaticBlockPosition(offset.block_offset);
    box_->Layer()->SetStaticInlinePosition(offset.inline_offset);
  }
}

}  // namespace blink
