// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGFragmentBuilder::NGFragmentBuilder(
    NGPhysicalFragmentBase::NGFragmentType type)
    : type_(type), writing_mode_(kHorizontalTopBottom), direction_(LTR) {}

NGFragmentBuilder& NGFragmentBuilder::SetWritingMode(
    NGWritingMode writing_mode) {
  writing_mode_ = writing_mode;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetDirection(TextDirection direction) {
  direction_ = direction;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineSize(LayoutUnit size) {
  size_.inline_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockSize(LayoutUnit size) {
  size_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetInlineOverflow(LayoutUnit size) {
  overflow_.inline_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockOverflow(LayoutUnit size) {
  overflow_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddChild(NGFragmentBase* child,
                                               NGLogicalOffset child_offset) {
  DCHECK_EQ(type_, NGPhysicalFragmentBase::kFragmentBox)
      << "Only box fragments can have children";
  children_.append(child->PhysicalFragment());
  offsets_.append(child_offset);
  // Collect child's out of flow descendants.
  const NGPhysicalFragmentBase* physical_fragment = child->PhysicalFragment();
  const Vector<NGStaticPosition>& oof_positions =
      physical_fragment->OutOfFlowPositions();
  size_t oof_index = 0;
  for (auto& oof_node : physical_fragment->OutOfFlowDescendants()) {
    NGStaticPosition oof_position = oof_positions[oof_index++];
    out_of_flow_descendant_candidates_.add(oof_node);
    out_of_flow_candidate_placements_.append(
        OutOfFlowPlacement{child_offset, oof_position});
  }
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode* child,
    NGLogicalOffset child_offset) {
  out_of_flow_descendant_candidates_.add(child);
  NGStaticPosition child_position =
      NGStaticPosition::Create(writing_mode_, direction_, NGPhysicalOffset());
  out_of_flow_candidate_placements_.append(
      OutOfFlowPlacement{child_offset, child_position});
  return *this;
}

void NGFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    WeakBoxList* descendants,
    Vector<NGStaticPosition>* descendant_positions) {
  DCHECK(descendants->isEmpty());
  DCHECK(descendant_positions->isEmpty());

  DCHECK_GE(size_.inline_size, LayoutUnit());
  DCHECK_GE(size_.block_size, LayoutUnit());
  NGPhysicalSize builder_physical_size{size_.ConvertToPhysical(writing_mode_)};

  size_t placement_index = 0;
  for (auto& oof_node : out_of_flow_descendant_candidates_) {
    OutOfFlowPlacement oof_placement =
        out_of_flow_candidate_placements_[placement_index++];

    NGPhysicalOffset child_offset =
        oof_placement.child_offset.ConvertToPhysical(
            writing_mode_, direction_, builder_physical_size, NGPhysicalSize());

    NGStaticPosition builder_relative_position;
    builder_relative_position.type = oof_placement.descendant_position.type;
    builder_relative_position.offset =
        child_offset + oof_placement.descendant_position.offset;
    descendants->add(oof_node);
    descendant_positions->append(builder_relative_position);
  }
  out_of_flow_descendant_candidates_.clear();
  out_of_flow_candidate_placements_.clear();
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowDescendant(
    NGBlockNode* descendant,
    const NGStaticPosition& position) {
  out_of_flow_descendants_.add(descendant);
  out_of_flow_positions_.append(position);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockStart(
    const NGMarginStrut& from) {
  margin_strut_.margin_block_start = from.margin_block_start;
  margin_strut_.negative_margin_block_start = from.negative_margin_block_start;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockEnd(
    const NGMarginStrut& from) {
  margin_strut_.margin_block_end = from.margin_block_end;
  margin_strut_.negative_margin_block_end = from.negative_margin_block_end;
  return *this;
}

NGPhysicalFragment* NGFragmentBuilder::ToFragment() {
  // TODO(layout-ng): Support text fragments
  DCHECK_EQ(type_, NGPhysicalFragmentBase::kFragmentBox);
  DCHECK_EQ(offsets_.size(), children_.size());

  NGPhysicalSize physical_size = size_.ConvertToPhysical(writing_mode_);
  HeapVector<Member<const NGPhysicalFragmentBase>> children;
  children.reserveCapacity(children_.size());

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragmentBase* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode_, direction_, physical_size, child->Size()));
    children.append(child);
  }
  return new NGPhysicalFragment(
      physical_size, overflow_.ConvertToPhysical(writing_mode_), children,
      out_of_flow_descendants_, out_of_flow_positions_, margin_strut_);
}

DEFINE_TRACE(NGFragmentBuilder) {
  visitor->trace(children_);
  visitor->trace(out_of_flow_descendant_candidates_);
  visitor->trace(out_of_flow_descendants_);
}

}  // namespace blink
