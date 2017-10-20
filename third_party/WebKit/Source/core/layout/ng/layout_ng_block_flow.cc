// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"

#include "core/layout/HitTestLocation.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_out_of_flow_layout_part.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/ng/ng_block_flow_painter.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

LayoutNGBlockFlow::~LayoutNGBlockFlow() {}

bool LayoutNGBlockFlow::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGBlockFlow || LayoutBlockFlow::IsOfType(type);
}

void LayoutNGBlockFlow::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  RefPtr<NGLayoutResult> result = NGBlockNode(this).Layout(*constraint_space);

  // We need to update our margins as these are calculated once and stored in
  // LayoutBox::margin_box_outsets_. Typically this happens within
  // UpdateLogicalWidth and UpdateLogicalHeight.
  //
  // This primarily fixes cases where we are embedded inside another layout,
  // for example LayoutView, LayoutFlexibleBox, etc.
  UpdateMargins(*constraint_space);

  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();

  NGPhysicalBoxFragment* fragment =
      ToNGPhysicalBoxFragment(result->PhysicalFragment().get());

  // This object has already been positioned in legacy layout by our containing
  // block. Copy the position and place the fragment.
  const LayoutBlock* containing_block = ContainingBlock();
  NGPhysicalOffset physical_offset;
  if (containing_block) {
    NGPhysicalSize containing_block_size(containing_block->Size().Width(),
                                         containing_block->Size().Height());
    NGLogicalOffset logical_offset(LogicalLeft(), LogicalTop());
    physical_offset = logical_offset.ConvertToPhysical(
        constraint_space->WritingMode(), constraint_space->Direction(),
        containing_block_size, fragment->Size());
  }
  fragment->SetOffset(physical_offset);
}

void LayoutNGBlockFlow::UpdateOutOfFlowBlockLayout() {
  LayoutBlock* container = ContainingBlock();
  const ComputedStyle* container_style = container->Style();
  const ComputedStyle* parent_style = Parent()->Style();
  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);
  NGFragmentBuilder container_builder(
      container, RefPtr<const ComputedStyle>(container_style),
      FromPlatformWritingMode(container_style->GetWritingMode()),
      container_style->Direction());

  // Compute ContainingBlock logical size.
  // OverrideContainingBlockLogicalWidth/Height are used by grid layout.
  // Override sizes are padding box size, not border box, so we must add
  // borders to compensate.
  NGBoxStrut borders;
  if (HasOverrideContainingBlockLogicalWidth() ||
      HasOverrideContainingBlockLogicalHeight())
    borders = ComputeBorders(*constraint_space, *container_style);

  LayoutUnit containing_block_logical_width;
  LayoutUnit containing_block_logical_height;
  if (HasOverrideContainingBlockLogicalWidth()) {
    containing_block_logical_width =
        OverrideContainingBlockContentLogicalWidth() + borders.InlineSum();
  } else {
    containing_block_logical_width = container->LogicalWidth();
  }
  if (HasOverrideContainingBlockLogicalHeight()) {
    containing_block_logical_height =
        OverrideContainingBlockContentLogicalHeight() + borders.BlockSum();
  } else {
    containing_block_logical_height = container->LogicalHeight();
  }

  container_builder.SetInlineSize(containing_block_logical_width);
  container_builder.SetBlockSize(containing_block_logical_height);

  // Determine static position.

  // static_inline and static_block are inline/block direction offsets
  // from physical origin. This is an unexpected blend of logical and
  // physical in a single variable.
  LayoutUnit static_inline;
  LayoutUnit static_block;

  if (container_style->IsDisplayFlexibleOrGridBox()) {
    static_inline = Layer()->StaticInlinePosition();
    static_block = Layer()->StaticBlockPosition();
  } else {
    Length logical_left;
    Length logical_right;
    Length logical_top;
    Length logical_bottom;
    ComputeInlineStaticDistance(logical_left, logical_right, this, container,
                                container->LogicalWidth());
    ComputeBlockStaticDistance(logical_top, logical_bottom, this, container);
    if (parent_style->IsLeftToRightDirection()) {
      if (!logical_left.IsAuto())
        static_inline = ValueForLength(logical_left, container->LogicalWidth());
    } else {
      if (!logical_right.IsAuto()) {
        static_inline =
            ValueForLength(logical_right, container->LogicalWidth());
      }
    }
    if (!logical_top.IsAuto())
      static_block = ValueForLength(logical_top, container->LogicalHeight());

    // Legacy static position is relative to padding box.
    // Convert to border box.
    NGBoxStrut border_strut =
        ComputeBorders(*constraint_space, *container_style);
    if (parent_style->IsLeftToRightDirection())
      static_inline += border_strut.inline_start;
    else
      static_inline -= border_strut.inline_end;
    static_block += border_strut.block_start;
  }
  if (!parent_style->IsLeftToRightDirection())
    static_inline = containing_block_logical_width - static_inline;
  if (parent_style->IsFlippedBlocksWritingMode())
    static_block = containing_block_logical_height - static_block;

  // Convert inline/block direction to physical direction. This can be done
  // with a simple coordinate flip because static_inline/block distances are
  // relative to physical origin.
  NGPhysicalOffset static_location =
      container_style->IsHorizontalWritingMode()
          ? NGPhysicalOffset(static_inline, static_block)
          : NGPhysicalOffset(static_block, static_inline);
  NGStaticPosition static_position = NGStaticPosition::Create(
      FromPlatformWritingMode(parent_style->GetWritingMode()),
      parent_style->Direction(), static_location);

  container_builder.AddOutOfFlowLegacyCandidate(NGBlockNode(this),
                                                static_position);

  // LayoutObject::ContainingBlock() is not equal to LayoutObject::Container()
  // when css container is inline. NG uses css container's style to determine
  // whether containing block can contain the node, and therefore must use
  // ::Container() because that object has the right Style().
  LayoutObject* css_container = Container();
  DCHECK(css_container->IsBox());
  NGOutOfFlowLayoutPart(NGBlockNode(ToLayoutBox(css_container)),
                        *constraint_space, *container_style, &container_builder)
      .Run(/* update_legacy */ false);
  RefPtr<NGLayoutResult> result = container_builder.ToBoxFragment();
  // These are the unpositioned OOF descendants of the current OOF block.
  for (NGOutOfFlowPositionedDescendant descendant :
       result->OutOfFlowPositionedDescendants())
    descendant.node.UseOldOutOfFlowPositioning();

  RefPtr<NGPhysicalBoxFragment> fragment =
      ToNGPhysicalBoxFragment(result->PhysicalFragment().get());
  DCHECK_GT(fragment->Children().size(), 0u);
  // Copy sizes of all child fragments to Legacy.
  // There could be multiple fragments, when this node has descendants whose
  // container is this node's container.
  // Example: fixed descendant of fixed element.
  for (RefPtr<NGPhysicalFragment> child_fragment : fragment->Children()) {
    DCHECK(child_fragment->GetLayoutObject()->IsBox());
    LayoutBox* child_legacy_box =
        ToLayoutBox(child_fragment->GetLayoutObject());
    NGPhysicalOffset child_offset = child_fragment->Offset();
    if (container_style->IsFlippedBlocksWritingMode()) {
      child_legacy_box->SetX(containing_block_logical_height -
                             child_offset.left - child_fragment->Size().width);
    } else {
      child_legacy_box->SetX(child_offset.left);
    }
    child_legacy_box->SetY(child_offset.top);
  }
  RefPtr<NGPhysicalFragment> child_fragment = fragment->Children()[0];
  DCHECK_EQ(fragment->Children()[0]->GetLayoutObject(), this);
}

void LayoutNGBlockFlow::UpdateMargins(
    const NGConstraintSpace& constraint_space) {
  SetMargin(ComputePhysicalMargins(constraint_space, StyleRef()));
}

NGInlineNodeData* LayoutNGBlockFlow::GetNGInlineNodeData() const {
  DCHECK(ng_inline_node_data_);
  return ng_inline_node_data_.get();
}

void LayoutNGBlockFlow::ResetNGInlineNodeData() {
  ng_inline_node_data_ = WTF::MakeUnique<NGInlineNodeData>();
}

void LayoutNGBlockFlow::WillCollectInlines() {}

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
const NGPhysicalBoxFragment* LayoutNGBlockFlow::CurrentFragment() const {
  if (cached_result_)
    return ToNGPhysicalBoxFragment(cached_result_->PhysicalFragment().get());
  return nullptr;
}

void LayoutNGBlockFlow::AddOverflowFromChildren() {
  // |ComputeOverflow()| calls this, which is called from
  // |CopyFragmentDataToLayoutBox()| and |RecalcOverflowAfterStyleChange()|.
  // Add overflow from the last layout cycle.
  if (ChildrenInline()) {
    if (const NGPhysicalBoxFragment* physical_fragment = CurrentFragment()) {
      // TODO(kojii): If |RecalcOverflowAfterStyleChange()|, we need to
      // re-compute glyph bounding box. How to detect it and how to re-compute
      // is TBD.
      AddContentsVisualOverflow(
          physical_fragment->ContentsVisualRect().ToLayoutRect());
      // TODO(kojii): The above code computes visual overflow only, we fallback
      // to LayoutBlock for AddLayoutOverflow() for now. It doesn't compute
      // correctly without RootInlineBox though.
    }
  }
  LayoutBlockFlow::AddOverflowFromChildren();
}

// Retrieve NGBaseline from the current fragment.
const NGBaseline* LayoutNGBlockFlow::FragmentBaseline(
    NGBaselineAlgorithmType type) const {
  if (const NGPhysicalFragment* physical_fragment = CurrentFragment()) {
    FontBaseline baseline_type =
        IsHorizontalWritingMode() ? kAlphabeticBaseline : kIdeographicBaseline;
    return ToNGPhysicalBoxFragment(physical_fragment)
        ->Baseline({type, baseline_type});
  }
  return nullptr;
}

LayoutUnit LayoutNGBlockFlow::FirstLineBoxBaseline() const {
  if (ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kFirstLine)) {
      return baseline->offset;
    }
  }
  return LayoutBlockFlow::FirstLineBoxBaseline();
}

LayoutUnit LayoutNGBlockFlow::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  if (ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kAtomicInline)) {
      return baseline->offset;
    }
  }
  return LayoutBlockFlow::InlineBlockBaseline(line_direction);
}

RefPtr<NGLayoutResult> LayoutNGBlockFlow::CachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token) const {
  if (!RuntimeEnabledFeatures::LayoutNGFragmentCachingEnabled())
    return nullptr;
  if (!cached_result_ || break_token || NeedsLayout())
    return nullptr;
  if (constraint_space != *cached_constraint_space_)
    return nullptr;
  return cached_result_->CloneWithoutOffset();
}

void LayoutNGBlockFlow::SetCachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token,
    RefPtr<NGLayoutResult> layout_result) {
  if (break_token || constraint_space.UnpositionedFloats().size() ||
      layout_result->UnpositionedFloats().size() ||
      layout_result->Status() != NGLayoutResult::kSuccess) {
    // We can't cache these yet
    return;
  }

  cached_constraint_space_ = &constraint_space;
  cached_result_ = layout_result;
}

RefPtr<NGLayoutResult> LayoutNGBlockFlow::CachedLayoutResultForTesting() {
  return cached_result_;
}

void LayoutNGBlockFlow::SetPaintFragment(
    RefPtr<const NGPhysicalFragment> fragment) {
  paint_fragment_ = WTF::MakeUnique<NGPaintFragment>(std::move(fragment));
}

void LayoutNGBlockFlow::Paint(const PaintInfo& paint_info,
                              const LayoutPoint& paint_offset) const {
  if (RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled() &&
      PaintFragment())
    NGBlockFlowPainter(*this).Paint(paint_info, paint_offset + Location());
  else
    LayoutBlockFlow::Paint(paint_info, paint_offset);
}

bool LayoutNGBlockFlow::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled() ||
      !PaintFragment()) {
    return LayoutBlockFlow::NodeAtPoint(result, location_in_container,
                                        accumulated_offset, action);
  }

  LayoutPoint adjusted_location = accumulated_offset + Location();
  if (!RootScrollerUtil::IsEffective(*this)) {
    // Check if we need to do anything at all.
    // If we have clipping, then we can't have any spillout.
    LayoutRect overflow_box =
        HasOverflowClip() ? BorderBoxRect() : VisualOverflowRect();
    overflow_box.MoveBy(adjusted_location);
    if (!location_in_container.Intersects(overflow_box))
      return false;
  }

  return NGBlockFlowPainter(*this).NodeAtPoint(result, location_in_container,
                                               accumulated_offset, action);
}

}  // namespace blink
