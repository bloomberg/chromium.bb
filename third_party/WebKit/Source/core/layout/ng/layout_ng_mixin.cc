// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_mixin.h"

#include "core/layout/HitTestLocation.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/ng/inline/ng_inline_fragment_iterator.h"
#include "core/layout/ng/inline/ng_inline_node_data.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/page/scrolling/RootScrollerUtil.h"
#include "core/paint/ng/ng_block_flow_painter.h"

namespace blink {

template <typename Base>
LayoutNGMixin<Base>::~LayoutNGMixin() {}

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
  ng_inline_node_data_ = WTF::MakeUnique<NGInlineNodeData>();
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
  return cached_result_->CloneWithoutOffset();
}

template <typename Base>
void LayoutNGMixin<Base>::SetCachedLayoutResult(
    const NGConstraintSpace& constraint_space,
    NGBreakToken* break_token,
    scoped_refptr<NGLayoutResult> layout_result) {
  if (break_token || constraint_space.UnpositionedFloats().size() ||
      layout_result->UnpositionedFloats().size() ||
      layout_result->Status() != NGLayoutResult::kSuccess) {
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
  paint_fragment_ = WTF::MakeUnique<NGPaintFragment>(std::move(fragment));
}

template <typename Base>
void LayoutNGMixin<Base>::Paint(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  if (RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled() &&
      PaintFragment())
    NGBlockFlowPainter(*this).Paint(paint_info,
                                    paint_offset + Base::Location());
  else
    LayoutBlockFlow::Paint(paint_info, paint_offset);
}

template <typename Base>
void LayoutNGMixin<Base>::UpdateMargins(
    const NGConstraintSpace& constraint_space) {
  Base::SetMargin(ComputePhysicalMargins(constraint_space, Base::StyleRef()));
}

template <typename Base>
bool LayoutNGMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_container,
    const LayoutPoint& accumulated_offset,
    HitTestAction action) {
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled()) {
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
                                               accumulated_offset, action);
}

template class LayoutNGMixin<LayoutBlockFlow>;

}  // namespace blink
