// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_line_box_fragment_builder.h"

#include "core/layout/ng/geometry/ng_logical_size.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_physical_line_box_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

NGLineBoxFragmentBuilder::NGLineBoxFragmentBuilder(NGInlineNode* node)
    : direction_(TextDirection::kLtr), node_(node) {}

NGLineBoxFragmentBuilder& NGLineBoxFragmentBuilder::SetDirection(
    TextDirection direction) {
  direction_ = direction;
  return *this;
}

NGLineBoxFragmentBuilder& NGLineBoxFragmentBuilder::SetInlineSize(
    LayoutUnit size) {
  inline_size_ = size;
  return *this;
}

NGLineBoxFragmentBuilder& NGLineBoxFragmentBuilder::AddChild(
    RefPtr<NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  children_.push_back(std::move(child));
  offsets_.push_back(child_offset);

  return *this;
}

void NGLineBoxFragmentBuilder::MoveChildrenInBlockDirection(LayoutUnit delta) {
  for (auto& offset : offsets_)
    offset.block_offset += delta;
}

void NGLineBoxFragmentBuilder::UniteMetrics(
    const NGLineHeightMetrics& metrics) {
  metrics_.Unite(metrics);
}

RefPtr<NGPhysicalLineBoxFragment>
NGLineBoxFragmentBuilder::ToLineBoxFragment() {
  DCHECK_EQ(offsets_.size(), children_.size());

  NGWritingMode writing_mode(
      FromPlatformWritingMode(node_->BlockStyle()->getWritingMode()));
  NGPhysicalSize physical_size =
      NGLogicalSize(inline_size_, Metrics().LineHeight())
          .ConvertToPhysical(writing_mode);

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode, direction_, physical_size, child->Size()));
  }

  // TODO(kojii): Implement BreakToken.
  return adoptRef(new NGPhysicalLineBoxFragment(physical_size, children_,
                                                metrics_, nullptr));
}

}  // namespace blink
