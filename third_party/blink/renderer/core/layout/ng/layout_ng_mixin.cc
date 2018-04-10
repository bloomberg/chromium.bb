// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/paint/ng/ng_block_flow_painter.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() = default;

template <typename Base>
bool LayoutNGMixin<Base>::IsOfType(LayoutObject::LayoutObjectType type) const {
  return type == LayoutObject::kLayoutObjectNGMixin || Base::IsOfType(type);
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

// The current fragment from the last layout cycle for this box.
// When pre-NG layout calls functions of this block flow, fragment and/or
// LayoutResult are required to compute the result.
// TODO(kojii): Use the cached result for now, we may need to reconsider as the
// cache evolves.
template <typename Base>
const NGPhysicalBoxFragment* LayoutNGMixin<Base>::CurrentFragment() const {
  if (cached_result_)
    return ToNGPhysicalBoxFragment(cached_result_->PhysicalFragment().get());
  return nullptr;
}

template <typename Base>
void LayoutNGMixin<Base>::AddOverflowFromChildren() {
  // |ComputeOverflow()| calls this, which is called from
  // |CopyFragmentDataToLayoutBox()| and |RecalcOverflowAfterStyleChange()|.
  // Add overflow from the last layout cycle.
  if (Base::ChildrenInline()) {
    if (const NGPhysicalBoxFragment* physical_fragment = CurrentFragment()) {
      bool has_width =
          physical_fragment->Style().OverflowX() != EOverflow::kHidden;
      bool has_height =
          physical_fragment->Style().OverflowY() != EOverflow::kHidden;
      if (has_width || has_height) {
        for (const auto& child : physical_fragment->Children()) {
          NGPhysicalOffsetRect child_rect(child->Offset(), child->Size());
          if (!has_width)
            child_rect.size.width = LayoutUnit();
          if (!has_height)
            child_rect.size.height = LayoutUnit();
          Base::AddLayoutOverflow(child_rect.ToLayoutRect());
        }
      }

      // TODO(kojii): If |RecalcOverflowAfterStyleChange()|, we need to
      // re-compute glyph bounding box. How to detect it and how to re-compute
      // is TBD.
      Base::AddContentsVisualOverflow(
          physical_fragment->ContentsVisualRect().ToLayoutRect());
      // TODO(kojii): The above code computes visual overflow only, we fallback
      // to LayoutBlock for AddLayoutOverflow() for now. It doesn't compute
      // correctly without RootInlineBox though.
    }
  }
  Base::AddOverflowFromChildren();
}

// Retrieve NGBaseline from the current fragment.
template <typename Base>
const NGBaseline* LayoutNGMixin<Base>::FragmentBaseline(
    NGBaselineAlgorithmType type) const {
  if (const NGPhysicalFragment* physical_fragment = CurrentFragment()) {
    FontBaseline baseline_type = Base::IsHorizontalWritingMode()
                                     ? kAlphabeticBaseline
                                     : kIdeographicBaseline;
    return ToNGPhysicalBoxFragment(physical_fragment)
        ->Baseline({type, baseline_type});
  }
  return nullptr;
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::FirstLineBoxBaseline() const {
  if (Base::ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kFirstLine)) {
      return baseline->offset;
    }
  }
  return Base::FirstLineBoxBaseline();
}

template <typename Base>
LayoutUnit LayoutNGMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  if (Base::ChildrenInline()) {
    if (const NGBaseline* baseline =
            FragmentBaseline(NGBaselineAlgorithmType::kAtomicInline)) {
      return baseline->offset;
    }
  }
  return Base::InlineBlockBaseline(line_direction);
}

template <typename Base>
scoped_refptr<NGLayoutResult> LayoutNGMixin<Base>::CachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token) const {
  if (!RuntimeEnabledFeatures::LayoutNGFragmentCachingEnabled())
    return nullptr;
  if (!cached_result_ || break_token || Base::NeedsLayout())
    return nullptr;
  if (constraint_space != *cached_constraint_space_)
    return nullptr;
  if (cached_constraint_space_->UnpositionedFloats().size() ||
      cached_result_->UnpositionedFloats().size())
    return nullptr;
  return cached_result_->CloneWithoutOffset();
}

template <typename Base>
void LayoutNGMixin<Base>::SetCachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token,
    scoped_refptr<NGLayoutResult> layout_result) {
  if (break_token || layout_result->Status() != NGLayoutResult::kSuccess) {
    // We can't cache these yet
    return;
  }

  cached_constraint_space_ = &constraint_space;
  cached_result_ = layout_result;
}

template <typename Base>
scoped_refptr<NGLayoutResult>
LayoutNGMixin<Base>::CachedLayoutResultForTesting() {
  return cached_result_;
}

template <typename Base>
void LayoutNGMixin<Base>::SetPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment) {
  paint_fragment_ = NGPaintFragment::Create(std::move(fragment));

  // When paint fragment is replaced, the subtree needs paint invalidation to
  // re-compute paint properties in NGPaintFragment.
  Base::SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kSubtree);
}

static Vector<NGPaintFragment*> GetNGPaintFragmentsInternal(
    NGPaintFragment* paint,
    const LayoutObject& layout_object) {
  if (!paint)
    return Vector<NGPaintFragment*>();
  Vector<NGPaintFragment*> fragments;
  if (paint->GetLayoutObject() == &layout_object)
    fragments.push_back(paint);
  for (const auto& child : paint->Children()) {
    const auto& result =
        GetNGPaintFragmentsInternal(child.get(), layout_object);
    fragments.AppendVector(result);
  }
  return fragments;
}

template <typename Base>
Vector<NGPaintFragment*> LayoutNGMixin<Base>::GetPaintFragments(
    const LayoutObject& layout_object) const {
  return GetNGPaintFragmentsInternal(PaintFragment(), layout_object);
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
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  if (PaintFragment())
    NGBlockFlowPainter(*this).Paint(paint_info, paint_offset);
  else
    LayoutBlockFlow::Paint(paint_info, paint_offset);
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  if (!PaintFragment()) {
    return LayoutBlockFlow::NodeAtPoint(result, location_in_container,
                                        accumulated_offset, action);
  }

  LayoutPoint adjusted_location = accumulated_offset + Base::Location();
  if (!RootScrollerUtil::IsEffective(*this)) {
    // Check if we need to do anything at all.
    // If we have clipping, then we can't have any spillout.
    LayoutRect overflow_box = Base::HasOverflowClip()
                                  ? Base::BorderBoxRect()
                                  : Base::VisualOverflowRect();
    overflow_box.MoveBy(adjusted_location);
    if (!location_in_container.Intersects(overflow_box))
      return false;
  }

  return NGBlockFlowPainter(*this).NodeAtPoint(result, location_in_container,
                                               accumulated_offset,
                                               accumulated_offset, action);
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

  if (!CurrentFragment())
    return Base::CreatePositionWithAffinity(0);

  const PositionWithAffinity ng_position =
      CurrentFragment()->PositionForPoint(NGPhysicalOffset(point));
  if (ng_position.IsNotNull())
    return ng_position;
  return Base::CreatePositionWithAffinity(0);
}

template class LayoutNGMixin<LayoutTableCell>;
template class LayoutNGMixin<LayoutBlockFlow>;

}  // namespace blink
