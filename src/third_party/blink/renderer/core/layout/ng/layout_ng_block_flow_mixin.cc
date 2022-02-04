// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow_mixin.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_progress.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_as_block.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_base.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_text.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

template <typename Base>
LayoutNGBlockFlowMixin<Base>::LayoutNGBlockFlowMixin(ContainerNode* node)
    : LayoutNGMixin<Base>(node) {
  static_assert(std::is_base_of<LayoutBlockFlow, Base>::value,
                "Base class of LayoutNGBlockFlowMixin must be LayoutBlockFlow "
                "or derived class.");
}

template <typename Base>
LayoutNGBlockFlowMixin<Base>::~LayoutNGBlockFlowMixin() = default;

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  Base::CheckIsNotDestroyed();
  Base::StyleDidChange(diff, old_style);

  if (diff.NeedsReshape()) {
    Base::SetNeedsCollectInlines();
  }
}

#if DCHECK_IS_ON()
template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddLayoutOverflowFromChildren() {
  Base::CheckIsNotDestroyed();
  NOTREACHED();
}
#endif

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::TakeNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  return ng_inline_node_data_.Release();
}

template <typename Base>
NGInlineNodeData* LayoutNGBlockFlowMixin<Base>::GetNGInlineNodeData() const {
  Base::CheckIsNotDestroyed();
  DCHECK(ng_inline_node_data_);
  return ng_inline_node_data_;
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ResetNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  ng_inline_node_data_ = MakeGarbageCollected<NGInlineNodeData>();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::ClearNGInlineNodeData() {
  Base::CheckIsNotDestroyed();
  if (ng_inline_node_data_) {
    // ng_inline_node_data_ is not used from now on but exists until GC happens,
    // so it is better to eagerly clear HeapVector to improve memory
    // utilization.
    ng_inline_node_data_->items.clear();
    ng_inline_node_data_.Clear();
  }
}

template <typename Base>
bool LayoutNGBlockFlowMixin<Base>::HasNGInlineNodeData() const {
  Base::CheckIsNotDestroyed();
  return ng_inline_node_data_;
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::AddOutlineRects(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  Base::CheckIsNotDestroyed();

  // TODO(crbug.com/1145048): Currently |NGBoxPhysicalFragment| does not support
  // NG block fragmentation. Fallback to the legacy code path.
  if (Base::PhysicalFragmentCount() == 1) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    if (fragment->HasItems()) {
      fragment->AddSelfOutlineRects(additional_offset, include_block_overflows,
                                    &rects);
      return;
    }
  }

  Base::AddOutlineRects(rects, additional_offset, include_block_overflows);
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::FirstLineBoxBaseline() const {
  Base::CheckIsNotDestroyed();

  if (const absl::optional<LayoutUnit> baseline =
          Base::FirstLineBoxBaselineOverride())
    return *baseline;

  // Return the baseline of the first fragment that has a baseline.
  // |OffsetFromOwnerLayoutBox| is not needed here, because the block offset of
  // all fragments are 0 for multicol.
  for (const NGPhysicalBoxFragment& fragment : Base::PhysicalFragments()) {
    if (const absl::optional<LayoutUnit> offset = fragment.Baseline())
      return *offset;
  }

  // This logic is in |LayoutBlock|, but we cannot call |Base| because doing so
  // may traverse |LayoutObject| tree, which may call this function for a child,
  // but the child may be block fragmented.
  if (Base::ChildrenInline()) {
    return Base::EmptyLineBaseline(
        Base::IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine);
  }
  return LayoutUnit(-1);
}

template <typename Base>
LayoutUnit LayoutNGBlockFlowMixin<Base>::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  Base::CheckIsNotDestroyed();

  // Please see |LayoutNGMixin<Base>::Paint()| for these DCHECKs.
  DCHECK(Base::GetNGPaginationBreakability() ==
             LayoutNGBlockFlow::kForbidBreaks ||
         !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  DCHECK_LE(Base::PhysicalFragmentCount(), 1u);

  if (const absl::optional<LayoutUnit> baseline =
          Base::InlineBlockBaselineOverride(line_direction))
    return *baseline;

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    if (absl::optional<LayoutUnit> offset = fragment->Baseline())
      return *offset;
  }

  // This logic is in |LayoutBlock| and |LayoutBlockFlow|, but we cannot call
  // |Base| because doing so may traverse |LayoutObject| tree, which may call
  // this function for a child, but the child may be block fragmented.
  if (!Base::ChildrenInline()) {
    for (LayoutObject* child = Base::LastChild(); child;
         child = child->PreviousSibling()) {
      DCHECK(child->IsBox());
      if (!child->IsFloatingOrOutOfFlowPositioned())
        return LayoutUnit(-1);
    }
  }
  return Base::EmptyLineBaseline(line_direction);
}

template <typename Base>
bool LayoutNGBlockFlowMixin<Base>::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    const PhysicalOffset& accumulated_offset,
    HitTestAction action) {
  Base::CheckIsNotDestroyed();

  // Please see |LayoutNGMixin<Base>::Paint()| for these DCHECKs.
  DCHECK(Base::GetNGPaginationBreakability() ==
             LayoutNGBlockFlow::kForbidBreaks ||
         !Base::CanTraversePhysicalFragments() ||
         !Base::Parent()->CanTraversePhysicalFragments());
  DCHECK_LE(Base::PhysicalFragmentCount(), 1u);

  if (!Base::MayIntersect(result, hit_test_location, accumulated_offset))
    return false;

  if (Base::PhysicalFragmentCount()) {
    const NGPhysicalBoxFragment* fragment = Base::GetPhysicalFragment(0);
    DCHECK(fragment);
    if (fragment->HasItems() ||
        // Check descendants of this fragment because floats may be in the
        // |NGFragmentItems| of the descendants.
        (action == kHitTestFloat &&
         fragment->HasFloatingDescendantsForPaint())) {
      return NGBoxFragmentPainter(*fragment).NodeAtPoint(
          result, hit_test_location, accumulated_offset, action);
    }
  }

  return LayoutBlockFlow::NodeAtPoint(result, hit_test_location,
                                      accumulated_offset, action);
}

template <typename Base>
PositionWithAffinity LayoutNGBlockFlowMixin<Base>::PositionForPoint(
    const PhysicalOffset& point) const {
  Base::CheckIsNotDestroyed();
  DCHECK_GE(Base::GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (Base::IsAtomicInlineLevel()) {
    const PositionWithAffinity atomic_inline_position =
        Base::PositionForPointIfOutsideAtomicInlineLevel(point);
    if (atomic_inline_position.IsNotNull())
      return atomic_inline_position;
  }

  if (!Base::ChildrenInline())
    return LayoutBlock::PositionForPoint(point);

  if (Base::PhysicalFragmentCount())
    return Base::PositionForPointInFragments(point);

  return Base::CreatePositionWithAffinity(0);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::DirtyLinesFromChangedChild(
    LayoutObject* child,
    MarkingBehavior marking_behavior) {
  Base::CheckIsNotDestroyed();
  DCHECK_EQ(marking_behavior, kMarkContainerChain);

  // We need to dirty line box fragments only if the child is once laid out in
  // LayoutNG inline formatting context. New objects are handled in
  // NGInlineNode::MarkLineBoxesDirty().
  if (child->IsInLayoutNGInlineFormattingContext())
    NGFragmentItems::DirtyLinesFromChangedChild(*child, *this);
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::UpdateNGBlockLayout() {
  Base::CheckIsNotDestroyed();

  if (Base::IsOutOfFlowPositioned()) {
    LayoutNGMixin<Base>::UpdateOutOfFlowBlockLayout();
    return;
  }

  LayoutNGMixin<Base>::UpdateInFlowBlockLayout();
  LayoutNGMixin<Base>::UpdateMargins();
}

template <typename Base>
void LayoutNGBlockFlowMixin<Base>::Trace(Visitor* visitor) const {
  visitor->Trace(ng_inline_node_data_);
  LayoutNGMixin<Base>::Trace(visitor);
}

template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutBlockFlow>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutProgress>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyAsBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyBase>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyRun>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutRubyText>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutSVGBlock>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutTableCaption>;
template class CORE_TEMPLATE_EXPORT LayoutNGBlockFlowMixin<LayoutView>;

}  // namespace blink
