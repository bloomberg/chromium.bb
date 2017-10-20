// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_container_fragment_builder.h"

#include "core/layout/ng/ng_exclusion_space.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGContainerFragmentBuilder::NGContainerFragmentBuilder(
    scoped_refptr<const ComputedStyle> style,
    NGWritingMode writing_mode,
    TextDirection direction)
    : NGBaseFragmentBuilder(std::move(style), writing_mode, direction) {}

NGContainerFragmentBuilder::~NGContainerFragmentBuilder() {}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::SetInlineSize(
    LayoutUnit inline_size) {
  inline_size_ = inline_size;
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::SetBfcOffset(
    const NGBfcOffset& bfc_offset) {
  bfc_offset_ = bfc_offset;
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::SetEndMarginStrut(
    const NGMarginStrut& end_margin_strut) {
  end_margin_strut_ = end_margin_strut;
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::SetExclusionSpace(
    std::unique_ptr<const NGExclusionSpace> exclusion_space) {
  exclusion_space_ = std::move(exclusion_space);
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::SwapUnpositionedFloats(
    Vector<scoped_refptr<NGUnpositionedFloat>>* unpositioned_floats) {
  unpositioned_floats_.swap(*unpositioned_floats);
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddChild(
    scoped_refptr<NGLayoutResult> child,
    const NGLogicalOffset& child_offset) {
  // Collect the child's out of flow descendants.
  for (const NGOutOfFlowPositionedDescendant& descendant :
       child->OutOfFlowPositionedDescendants()) {
    oof_positioned_candidates_.push_back(
        NGOutOfFlowPositionedCandidate{descendant, child_offset});
  }

  return AddChild(child->PhysicalFragment(), child_offset);
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddChild(
    scoped_refptr<NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  children_.push_back(std::move(child));
  offsets_.push_back(child_offset);
  return *this;
}

NGContainerFragmentBuilder& NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    NGOutOfFlowPositionedDescendant descendant) {
  oof_positioned_descendants_.push_back(descendant);
  return *this;
}

}  // namespace blink
