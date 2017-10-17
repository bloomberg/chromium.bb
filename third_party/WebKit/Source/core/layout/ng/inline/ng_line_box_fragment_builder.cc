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
    RefPtr<const ComputedStyle> style,
    NGWritingMode writing_mode)
    : NGContainerFragmentBuilder(style, writing_mode, TextDirection::kLtr),
      node_(node) {}

NGLogicalSize NGLineBoxFragmentBuilder::Size() const {
  return {inline_size_, metrics_.LineHeight()};
}

void NGLineBoxFragmentBuilder::MoveChildrenInBlockDirection(LayoutUnit delta) {
  for (auto& offset : offsets_)
    offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::MoveChildrenInBlockDirection(LayoutUnit delta,
                                                            unsigned start,
                                                            unsigned end) {
  for (unsigned index = start; index < end; index++)
    offsets_[index].block_offset += delta;
}

void NGLineBoxFragmentBuilder::SetMetrics(const NGLineHeightMetrics& metrics) {
  metrics_ = metrics;
}

void NGLineBoxFragmentBuilder::AddPositionedFloat(
    const NGPositionedFloat& positioned_float) {
  positioned_floats_.push_back(positioned_float);
}

void NGLineBoxFragmentBuilder::SetBreakToken(
    RefPtr<NGInlineBreakToken> break_token) {
  break_token_ = std::move(break_token);
}

RefPtr<NGLayoutResult> NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  DCHECK_EQ(offsets_.size(), children_.size());

  NGWritingMode writing_mode(
      FromPlatformWritingMode(node_.Style().GetWritingMode()));
  NGPhysicalSize physical_size =
      NGLogicalSize(inline_size_, Metrics().LineHeight())
          .ConvertToPhysical(writing_mode);

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode, Direction(), physical_size, child->Size()));
  }

  RefPtr<NGPhysicalLineBoxFragment> fragment =
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
