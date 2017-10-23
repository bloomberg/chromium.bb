// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_exclusion_space.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_positioned_float.h"

namespace blink {

NGLineBoxFragmentBuilder::NGLineBoxFragmentBuilder(
    NGInlineNode node,
    scoped_refptr<const ComputedStyle> style,
    NGWritingMode writing_mode,
    TextDirection)
    : NGContainerFragmentBuilder(style, writing_mode, TextDirection::kLtr),
      node_(node) {}

NGLineBoxFragmentBuilder::~NGLineBoxFragmentBuilder() {}

NGLogicalSize NGLineBoxFragmentBuilder::Size() const {
  return {inline_size_, metrics_.LineHeight().ClampNegativeToZero()};
}

const NGPhysicalFragment* NGLineBoxFragmentBuilder::Child::PhysicalFragment()
    const {
  return layout_result ? layout_result->PhysicalFragment().get()
                       : fragment.get();
}

void NGLineBoxFragmentBuilder::ChildList::AddChild(
    scoped_refptr<NGLayoutResult> layout_result,
    const NGLogicalOffset& child_offset) {
  children_.push_back(Child{std::move(layout_result), nullptr, child_offset});
}

void NGLineBoxFragmentBuilder::ChildList::AddChild(
    scoped_refptr<NGPhysicalFragment> fragment,
    const NGLogicalOffset& child_offset) {
  children_.push_back(Child{nullptr, std::move(fragment), child_offset});
}

void NGLineBoxFragmentBuilder::ChildList::AddChild(
    std::nullptr_t,
    const NGLogicalOffset& child_offset) {
  children_.push_back(Child{nullptr, nullptr, child_offset});
}

void NGLineBoxFragmentBuilder::ChildList::MoveInBlockDirection(
    LayoutUnit delta) {
  for (auto& child : children_)
    child.offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::ChildList::MoveInBlockDirection(LayoutUnit delta,
                                                               unsigned start,
                                                               unsigned end) {
  for (unsigned index = start; index < end; index++)
    children_[index].offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::SetMetrics(const NGLineHeightMetrics& metrics) {
  metrics_ = metrics;
}

void NGLineBoxFragmentBuilder::SetBlockSize(LayoutUnit block_size) {
  block_size_ = block_size;
}

void NGLineBoxFragmentBuilder::AddPositionedFloat(
    const NGPositionedFloat& positioned_float) {
  positioned_floats_.push_back(positioned_float);
}

void NGLineBoxFragmentBuilder::SetBreakToken(
    scoped_refptr<NGInlineBreakToken> break_token) {
  break_token_ = std::move(break_token);
}

void NGLineBoxFragmentBuilder::AddChildren(ChildList& children) {
  offsets_.ReserveCapacity(children.size());
  children_.ReserveCapacity(children.size());

  for (auto& child : children) {
    if (child.layout_result) {
      DCHECK(!child.fragment);
      AddChild(std::move(child.layout_result), child.offset);
      DCHECK(!child.layout_result);
    } else {
      DCHECK(child.fragment);
      AddChild(std::move(child.fragment), child.offset);
      DCHECK(!child.fragment);
    }
  }
}

scoped_refptr<NGLayoutResult> NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  DCHECK_EQ(offsets_.size(), children_.size());

  NGWritingMode writing_mode(
      FromPlatformWritingMode(node_.Style().GetWritingMode()));
  NGPhysicalSize physical_size =
      NGLogicalSize(inline_size_, block_size_).ConvertToPhysical(writing_mode);

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode, Direction(), physical_size, child->Size()));
  }

  scoped_refptr<NGPhysicalLineBoxFragment> fragment =
      WTF::AdoptRef(new NGPhysicalLineBoxFragment(
          Style(), physical_size, children_, metrics_,
          break_token_ ? std::move(break_token_)
                       : NGInlineBreakToken::Create(node_)));

  return WTF::AdoptRef(new NGLayoutResult(
      std::move(fragment), oof_positioned_descendants_, positioned_floats_,
      unpositioned_floats_, std::move(exclusion_space_), bfc_offset_,
      end_margin_strut_,
      /* intrinsic_block_size */ LayoutUnit(), NGLayoutResult::kSuccess));
}

}  // namespace blink
