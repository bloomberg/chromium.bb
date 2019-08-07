// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::LayoutNGMixin(Element* element) : Base(element) {
  static_assert(
      std::is_base_of<LayoutBlockFlow, Base>::value,
      "Base class of LayoutNGMixin must be LayoutBlockFlow or derived class.");
  DCHECK(!element || !element->ShouldForceLegacyLayout());
}

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
bool LayoutNGMixin<Base>::IsOfType(LayoutObject::LayoutObjectType type) const {
  return type == LayoutObject::kLayoutObjectNGMixin || Base::IsOfType(type);
}

template <typename Base>
void LayoutNGMixin<Base>::StyleDidChange(StyleDifference diff,
                                         const ComputedStyle* old_style) {
  Base::StyleDidChange(diff, old_style);

  const ComputedStyle& new_style = Base::StyleRef();
  if (old_style && Base::ChildrenInline() &&
      new_style.GetUnicodeBidi() != old_style->GetUnicodeBidi()) {
    Base::SetNeedsCollectInlines();
  }
}

template <typename Base>
NGInlineNodeData* LayoutNGMixin<Base>::TakeNGInlineNodeData() {
  return ng_inline_node_data_.release();
}

template <typename Base>
NGInlineNodeData* LayoutNGMixin<Base>::GetNGInlineNodeData() const {
  DCHECK(ng_inline_node_data_);
  return ng_inline_node_data_.get();
}

template <typename Base>
void LayoutNGMixin<Base>::ResetNGInlineNodeData() {
  ng_inline_node_data_ = std::make_unique<NGInlineNodeData>();
}

template <typename Base>
void LayoutNGMixin<Base>::ClearNGInlineNodeData() {
  ng_inline_node_data_.reset();
}

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
template <typename Base>
const NGPhysicalBoxFragment* LayoutNGMixin<Base>::CurrentFragment() const {
  const NGLayoutResult* cached_layout_result = Base::GetCachedLayoutResult();
  if (!cached_layout_result)
    return nullptr;

  return To<NGPhysicalBoxFragment>(cached_layout_result->PhysicalFragment());
}

template <typename Base>
void LayoutNGMixin<Base>::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  NGBlockNode node(const_cast<LayoutNGMixin<Base>*>(this));
  if (!node.CanUseNewLayout()) {
    Base::ComputeIntrinsicLogicalWidths(min_logical_width, max_logical_width);
    return;
  }

  LayoutUnit available_logical_height =
      LayoutBoxUtils::AvailableLogicalHeight(*this, Base::ContainingBlock());
  MinMaxSizeInput input(available_logical_height);
  // This function returns content-box plus scrollbar.
  input.size_type = NGMinMaxSizeType::kContentBoxSize;
  MinMaxSize sizes =
      node.ComputeMinMaxSize(node.Style().GetWritingMode(), input);

  if (Base::IsTableCell()) {
    // If a table cell, or the column that it belongs to, has a specified fixed
    // positive inline-size, and the measured intrinsic max size is less than
    // that, use specified size as max size.
    LayoutTableCell* cell = ToLayoutTableCell(node.GetLayoutBox());
    Length table_cell_width = cell->StyleOrColLogicalWidth();
    if (table_cell_width.IsFixed() && table_cell_width.Value() > 0) {
      sizes.max_size = std::max(sizes.min_size,
                                Base::AdjustContentBoxLogicalWidthForBoxSizing(
                                    LayoutUnit(table_cell_width.Value())));
    }
  }

  sizes += LayoutUnit(Base::ScrollbarLogicalWidth());
  min_logical_width = sizes.min_size;
  max_logical_width = sizes.max_size;
}

template <typename Base>
void LayoutNGMixin<Base>::AddLayoutOverflowFromChildren() {
  // |ComputeOverflow()| calls this, which is called from
  // |CopyFragmentDataToLayoutBox()| and |RecalcOverflow()|.
  // Add overflow from the last layout cycle.
  // TODO(chrishtr): do we need to condition on CurrentFragment()? Why?
  if (CurrentFragment()) {
    AddScrollingOverflowFromChildren();
  }
  Base::AddLayoutOverflowFromChildren();
}

template <typename Base>
void LayoutNGMixin<Base>::AddScrollingOverflowFromChildren() {
  bool children_inline = Base::ChildrenInline();

  const NGPhysicalBoxFragment* physical_fragment = CurrentFragment();
  DCHECK(physical_fragment);
  // inline-end LayoutOverflow padding spec is still undecided:
  // https://github.com/w3c/csswg-drafts/issues/129
  // For backwards compatibility, if container clips overflow,
  // padding is added to the inline-end for inline children.
  base::Optional<NGPhysicalBoxStrut> padding_strut;
  if (Base::HasOverflowClip()) {
    padding_strut =
        NGBoxStrut(LayoutUnit(), Base::PaddingEnd(), LayoutUnit(), LayoutUnit())
            .ConvertToPhysical(Base::StyleRef().GetWritingMode(),
                               Base::StyleRef().Direction());
  }

  NGPhysicalOffsetRect children_overflow;

  // Only add overflow for fragments NG has not reflected into Legacy.
  // These fragments are:
  // - inline fragments,
  // - out of flow fragments whose css container is inline box.
  // TODO(layout-dev) Transfroms also need to be applied to compute overflow
  // correctly. NG is not yet transform-aware. crbug.com/855965
  if (!physical_fragment->Children().IsEmpty()) {
    for (const auto& child : physical_fragment->Children()) {
      NGPhysicalOffsetRect child_scrollable_overflow;
      if (child->IsOutOfFlowPositioned()) {
        child_scrollable_overflow =
            child->ScrollableOverflowForPropagation(this);
      } else if (children_inline && child->IsLineBox()) {
        DCHECK(child->IsLineBox());
        child_scrollable_overflow =
            To<NGPhysicalLineBoxFragment>(*child).ScrollableOverflow(
                this, Base::Style(), physical_fragment->Size());
        if (padding_strut)
          child_scrollable_overflow.Expand(*padding_strut);
      } else {
        continue;
      }
      child_scrollable_overflow.offset += child.Offset();
      children_overflow.Unite(child_scrollable_overflow);
    }
  }

  // LayoutOverflow takes flipped blocks coordinates, adjust as needed.
  LayoutRect children_flipped_overflow = children_overflow.ToLayoutFlippedRect(
      physical_fragment->Style(), physical_fragment->Size());
  Base::AddLayoutOverflow(children_flipped_overflow);
}

template <typename Base>
void LayoutNGMixin<Base>::AddOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint& additional_offset,
    NGOutlineType include_block_overflows) const {
  if (PaintFragment()) {
    PaintFragment()->AddSelfOutlineRect(&rects, additional_offset,
                                        include_block_overflows);
  } else {
    Base::AddOutlineRects(rects, additional_offset, include_block_overflows);
  }
}

// Retrieve NGBaseline from the current fragment.
template <typename Base>
base::Optional<LayoutUnit> LayoutNGMixin<Base>::FragmentBaseline(
    NGBaselineAlgorithmType type) const {
  if (Base::ShouldApplyLayoutContainment())
    return base::nullopt;

  if (const NGPhysicalFragment* physical_fragment = CurrentFragment()) {
    FontBaseline baseline_type = Base::StyleRef().GetFontBaseline();
    return To<NGPhysicalBoxFragment>(physical_fragment)
        ->Baseline({type, baseline_type});
  }
  return base::nullopt;
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::FirstLineBoxBaseline() const {
  if (Base::ChildrenInline()) {
    if (base::Optional<LayoutUnit> offset =
            FragmentBaseline(NGBaselineAlgorithmType::kFirstLine)) {
      return *offset;
    }
  }
  return Base::FirstLineBoxBaseline();
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  if (Base::ChildrenInline()) {
    if (base::Optional<LayoutUnit> offset =
            FragmentBaseline(NGBaselineAlgorithmType::kAtomicInline)) {
      return *offset;
    }
  }
  return Base::InlineBlockBaseline(line_direction);
}

template <typename Base>
void LayoutNGMixin<Base>::SetPaintFragment(
    const NGBlockBreakToken* break_token,
    scoped_refptr<const NGPhysicalFragment> fragment) {
  DCHECK(!break_token || break_token->InputNode().GetLayoutBox() == this);

  scoped_refptr<NGPaintFragment>* current =
      NGPaintFragment::Find(&paint_fragment_, break_token);
  DCHECK(current);
  const NGPaintFragment* old = current->get();
  if (fragment) {
    *current = NGPaintFragment::Create(std::move(fragment), break_token,
                                       std::move(*current));
  } else {
    *current = nullptr;
  }

  if (old && old != current->get()) {
    // Painting layer needs repaint when a DisplayItemClient is destroyed.
    // TODO(kojii): We need this here for now, but this should be handled
    // differently for better efficiency, in pre-paint tree walk to walk
    // fragment tree, or before layout. crbug.com/941228
    ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
  }
}

template <typename Base>
void LayoutNGMixin<Base>::InvalidateDisplayItemClients(
    PaintInvalidationReason invalidation_reason) const {
  if (NGPaintFragment* fragment = PaintFragment()) {
    // TODO(koji): Should be in the PaintInvalidator, possibly with more logic
    // ported from BlockFlowPaintInvalidator.
    ObjectPaintInvalidator object_paint_invalidator(*this);
    object_paint_invalidator.InvalidateDisplayItemClient(*fragment,
                                                         invalidation_reason);
    return;
  }

  LayoutBlockFlow::InvalidateDisplayItemClients(invalidation_reason);
}

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info) const {
  if (const NGPaintFragment* paint_fragment = PaintFragment())
    NGBoxFragmentPainter(*paint_fragment).Paint(paint_info);
  else
    LayoutBlockFlow::Paint(paint_info);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  const NGPaintFragment* paint_fragment = PaintFragment();
  if (!paint_fragment) {
    return LayoutBlockFlow::NodeAtPoint(result, location_in_container,
                                        accumulated_offset, action);
  }
  // In LayoutBox::NodeAtPoint() and subclass overrides, it is guaranteed that
  // |accumulated_offset + Location()| equals the physical offset of the current
  // LayoutBox in the paint layer, regardless of writing mode or whether the box
  // was placed by NG or legacy.
  const LayoutPoint physical_offset = accumulated_offset + Base::Location();
  if (!this->IsEffectiveRootScroller()) {
    // Check if we need to do anything at all.
    // If we have clipping, then we can't have any spillout.
    LayoutRect overflow_box = Base::HasOverflowClip()
                                  ? Base::BorderBoxRect()
                                  : Base::VisualOverflowRect();
    overflow_box.MoveBy(physical_offset);
    if (!location_in_container.Intersects(overflow_box))
      return false;
  }
  if (Base::IsInSelfHitTestingPhase(action) && Base::HasOverflowClip() &&
      Base::HitTestOverflowControl(result, location_in_container,
                                   physical_offset))
    return true;

  return NGBoxFragmentPainter(*paint_fragment)
      .NodeAtPoint(result, location_in_container, physical_offset, action);
}

template <typename Base>
PositionWithAffinity LayoutNGMixin<Base>::PositionForPoint(
    const LayoutPoint& point) const {
  if (Base::IsAtomicInlineLevel()) {
    const PositionWithAffinity atomic_inline_position =
        Base::PositionForPointIfOutsideAtomicInlineLevel(point);
    if (atomic_inline_position.IsNotNull())
      return atomic_inline_position;
  }

  if (!Base::ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  if (!PaintFragment())
    return Base::CreatePositionWithAffinity(0);

  const PositionWithAffinity ng_position =
      PaintFragment()->PositionForPoint(NGPhysicalOffset(point));
  if (ng_position.IsNotNull())
    return ng_position;
  return Base::CreatePositionWithAffinity(0);
}

template <typename Base>
void LayoutNGMixin<Base>::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  DCHECK_EQ(marking_behavior, kMarkContainerChain);

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // NGInlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext())
    NGPaintFragment::DirtyLinesFromChangedChild(child);
}

template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutTableCell>;
template class CORE_TEMPLATE_EXPORT LayoutNGMixin<LayoutBlockFlow>;

}  // namespace blink
