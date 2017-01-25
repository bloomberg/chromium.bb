// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_text_fragment.h"

namespace blink {

// TODO(ikilpatrick): Make writing mode and direction be in the constructor.
NGFragmentBuilder::NGFragmentBuilder(NGPhysicalFragment::NGFragmentType type,
                                     LayoutObject* layout_object)
    : type_(type),
      writing_mode_(kHorizontalTopBottom),
      direction_(TextDirection::kLtr),
      layout_object_(layout_object) {}

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

NGFragmentBuilder& NGFragmentBuilder::AddChild(
    NGFragment* child,
    const NGLogicalOffset& child_offset) {
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentBox)
      << "Only box fragments can have children";
  children_.push_back(child->PhysicalFragment());
  offsets_.push_back(child_offset);
  // Collect child's out of flow descendants.
  const NGPhysicalFragment* physical_fragment = child->PhysicalFragment();
  const Vector<NGStaticPosition>& oof_positions =
      physical_fragment->OutOfFlowPositions();
  size_t oof_index = 0;
  for (auto& oof_node : physical_fragment->OutOfFlowDescendants()) {
    NGStaticPosition oof_position = oof_positions[oof_index++];
    out_of_flow_descendant_candidates_.add(oof_node);
    out_of_flow_candidate_placements_.push_back(
        OutOfFlowPlacement{child_offset, oof_position});
  }
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddFloatingObject(
    NGFloatingObject* floating_object,
    const NGLogicalOffset& floating_object_offset) {
  positioned_floats_.append(floating_object);
  floating_object_offsets_.append(floating_object_offset);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode* child,
    NGLogicalOffset child_offset) {
  out_of_flow_descendant_candidates_.add(child);
  NGStaticPosition child_position =
      NGStaticPosition::Create(writing_mode_, direction_, NGPhysicalOffset());
  out_of_flow_candidate_placements_.push_back(
      OutOfFlowPlacement{child_offset, child_position});
  child->SaveStaticOffsetForLegacy(child_offset);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddUnpositionedFloat(
    NGFloatingObject* floating_object) {
  unpositioned_floats_.append(floating_object);
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
    descendant_positions->push_back(builder_relative_position);
  }
  out_of_flow_descendant_candidates_.clear();
  out_of_flow_candidate_placements_.clear();
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowDescendant(
    NGBlockNode* descendant,
    const NGStaticPosition& position) {
  out_of_flow_descendants_.add(descendant);
  out_of_flow_positions_.push_back(position);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockStart(
    const NGDeprecatedMarginStrut& from) {
  margin_strut_.margin_block_start = from.margin_block_start;
  margin_strut_.negative_margin_block_start = from.negative_margin_block_start;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetMarginStrutBlockEnd(
    const NGDeprecatedMarginStrut& from) {
  margin_strut_.margin_block_end = from.margin_block_end;
  margin_strut_.negative_margin_block_end = from.negative_margin_block_end;
  return *this;
}

NGPhysicalBoxFragment* NGFragmentBuilder::ToBoxFragment() {
  // TODO(layout-ng): Support text fragments
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentBox);
  DCHECK_EQ(offsets_.size(), children_.size());

  auto* break_token = break_token_.get();
  break_token_ = nullptr;

  NGPhysicalSize physical_size = size_.ConvertToPhysical(writing_mode_);
  HeapVector<Member<NGPhysicalFragment>> children;
  children.reserveCapacity(children_.size());

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode_, direction_, physical_size, child->Size()));
    children.push_back(child);
  }

  HeapVector<Member<NGFloatingObject>> positioned_floats;
  positioned_floats.reserveCapacity(positioned_floats_.size());

  for (size_t i = 0; i < positioned_floats_.size(); ++i) {
    Member<NGFloatingObject>& floating_object = positioned_floats_[i];
    NGPhysicalFragment* floating_fragment = floating_object->fragment;
    floating_fragment->SetOffset(floating_object_offsets_[i].ConvertToPhysical(
        writing_mode_, direction_, physical_size, floating_fragment->Size()));
    positioned_floats.append(floating_object);
  }

  return new NGPhysicalBoxFragment(
      layout_object_, physical_size, overflow_.ConvertToPhysical(writing_mode_),
      children, out_of_flow_descendants_, out_of_flow_positions_, margin_strut_,
      unpositioned_floats_, positioned_floats_, break_token);
}

NGPhysicalTextFragment* NGFragmentBuilder::ToTextFragment(NGInlineNode* node,
                                                          unsigned start_index,
                                                          unsigned end_index) {
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentText);
  DCHECK(children_.isEmpty());
  DCHECK(offsets_.isEmpty());

  HeapVector<Member<NGFloatingObject>> empty_unpositioned_floats;
  HeapVector<Member<NGFloatingObject>> empty_positioned_floats;

  return new NGPhysicalTextFragment(
      layout_object_, node, start_index, end_index,
      size_.ConvertToPhysical(writing_mode_),
      overflow_.ConvertToPhysical(writing_mode_), out_of_flow_descendants_,
      out_of_flow_positions_, empty_unpositioned_floats,
      empty_positioned_floats);
}

DEFINE_TRACE(NGFragmentBuilder) {
  visitor->trace(children_);
  visitor->trace(out_of_flow_descendant_candidates_);
  visitor->trace(out_of_flow_descendants_);
  visitor->trace(positioned_floats_);
  visitor->trace(unpositioned_floats_);
  visitor->trace(break_token_);
}

}  // namespace blink
