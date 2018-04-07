// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

NGLogicalOffset ChildLogicalOffsetInParent(
    const NGPhysicalContainerFragment& parent,
    const NGPhysicalFragment& child) {
  return child.Offset().ConvertToLogical(parent.Style().GetWritingMode(),
                                         parent.Style().Direction(),
                                         parent.Size(), child.Size());
}

NGLogicalSize ChildLogicalSizeInParent(
    const NGPhysicalContainerFragment& parent,
    const NGPhysicalFragment& child) {
  return NGFragment(parent.Style().GetWritingMode(), child).Size();
}

Optional<PositionWithAffinity> PositionForPointInChild(
    const NGPhysicalFragment& child,
    const NGPhysicalOffset& point) {
  const NGPhysicalOffset& child_point = point - child.Offset();
  // We must fallback to legacy for old layout roots. We also fallback (to
  // LayoutNGMixin::PositionForPoint()) for NG block layout, so that we can
  // utilize LayoutBlock::PositionForPoint() that resolves the position in block
  // layout.
  // TODO(xiaochengh): Don't fallback to legacy for NG block layout.
  const PositionWithAffinity result =
      (child.IsBlockFlow() || child.IsOldLayoutRoot())
          ? child.GetLayoutObject()->PositionForPoint(
                child_point.ToLayoutPoint())
          : child.PositionForPoint(child_point);
  if (result.IsNotNull())
    return result;
  return WTF::nullopt;
}

}  // namespace

NGPhysicalContainerFragment::NGPhysicalContainerFragment(
    LayoutObject* layout_object,
    const ComputedStyle& style,
    NGPhysicalSize size,
    NGFragmentType type,
    unsigned sub_type,
    Vector<scoped_refptr<NGPhysicalFragment>>& children,
    const NGPhysicalOffsetRect& contents_visual_rect,
    scoped_refptr<NGBreakToken> break_token)
    : NGPhysicalFragment(layout_object,
                         style,
                         NGStyleVariant::kStandard,
                         size,
                         type,
                         sub_type,
                         std::move(break_token)),
      children_(std::move(children)),
      contents_visual_rect_(contents_visual_rect) {
  DCHECK(children.IsEmpty());  // Ensure move semantics is used.
}

PositionWithAffinity
NGPhysicalContainerFragment::PositionForPointInInlineLevelBox(
    const NGPhysicalOffset& point) const {
  DCHECK(IsInline() || IsLineBox()) << ToString();
  DCHECK(!IsBlockFlow()) << ToString();

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(), NGPhysicalSize());
  const LayoutUnit inline_point = logical_point.inline_offset;

  // Stores the closest child before |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPhysicalFragment* closest_child_before = nullptr;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPhysicalFragment* closest_child_after = nullptr;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  for (const auto& child : children_) {
    const LayoutUnit child_inline_min =
        ChildLogicalOffsetInParent(*this, *child).inline_offset;
    const LayoutUnit child_inline_max =
        child_inline_min + ChildLogicalSizeInParent(*this, *child).inline_size;

    // Try to resolve if |point| falls in any child in inline direction.
    if (inline_point >= child_inline_min && inline_point <= child_inline_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (inline_point < child_inline_min) {
      if (child_inline_min < closest_child_after_inline_offset) {
        closest_child_after = child.get();
        closest_child_after_inline_offset = child_inline_min;
      }
    }

    if (inline_point > child_inline_max) {
      if (child_inline_max > closest_child_before_inline_offset) {
        closest_child_before = child.get();
        closest_child_before_inline_offset = child_inline_max;
      }
    }
  }

  if (closest_child_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_after, point))
      return child_position.value();
  }

  if (closest_child_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_before, point))
      return child_position.value();
  }

  return PositionWithAffinity();
}

PositionWithAffinity
NGPhysicalContainerFragment::PositionForPointInInlineFormattingContext(
    const NGPhysicalOffset& point) const {
  DCHECK(IsBlockFlow()) << ToString();
  DCHECK(IsBox()) << ToString();
  DCHECK(ToNGPhysicalBoxFragment(this)->ChildrenInline()) << ToString();

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(), NGPhysicalSize());
  const LayoutUnit block_point = logical_point.block_offset;

  // Stores the closest line box child above |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPhysicalLineBoxFragment* closest_line_before = nullptr;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Min();

  // Stores the closest line box child below |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPhysicalLineBoxFragment* closest_line_after = nullptr;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Max();

  for (const auto& child : children_) {
    // Try to resolve if |point| falls in a non-line-box child completely.
    if (!child->IsLineBox()) {
      if (point.left >= child->Offset().left &&
          point.left <= child->Offset().left + child->Size().width &&
          point.top >= child->Offset().top &&
          point.top <= child->Offset().top + child->Size().height) {
        if (auto child_position = PositionForPointInChild(*child, point))
          return child_position.value();
      }
      continue;
    }

    if (!child->IsLineBox() ||
        ToNGPhysicalLineBoxFragment(*child).Children().IsEmpty())
      continue;

    const LayoutUnit line_min =
        ChildLogicalOffsetInParent(*this, *child).block_offset;
    const LayoutUnit line_max =
        line_min + ChildLogicalSizeInParent(*this, *child).block_size;

    // Try to resolve if |point| falls in a line box in block direction.
    // Hitting on line bottom doesn't count, to match legacy behavior.
    // TODO(xiaochengh): Consider floats.
    if (block_point >= line_min && block_point < line_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (block_point < line_min) {
      if (line_min < closest_line_after_block_offset) {
        closest_line_after = ToNGPhysicalLineBoxFragment(child.get());
        closest_line_after_block_offset = line_min;
      }
    }

    if (block_point >= line_max) {
      if (line_max > closest_line_before_block_offset) {
        closest_line_before = ToNGPhysicalLineBoxFragment(child.get());
        closest_line_before_block_offset = line_max;
      }
    }
  }

  if (closest_line_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_after, point))
      return child_position.value();
  }

  if (closest_line_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_before, point))
      return child_position.value();
  }

  // TODO(xiaochengh): Looking at only the closest lines may not be enough,
  // when we have multiple lines full of pseudo elements. Fix it.

  // TODO(xiaochengh): Consider floats.

  return PositionWithAffinity();
}

}  // namespace blink
