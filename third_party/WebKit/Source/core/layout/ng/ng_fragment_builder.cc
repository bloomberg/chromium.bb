// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "platform/heap/Handle.h"

namespace blink {

// TODO(ikilpatrick): Make writing mode and direction be in the constructor.
NGFragmentBuilder::NGFragmentBuilder(NGPhysicalFragment::NGFragmentType type,
                                     NGLayoutInputNode* node)
    : type_(type),
      writing_mode_(kHorizontalTopBottom),
      direction_(TextDirection::kLtr),
      node_(node),
      did_break_(false) {
}

NGFragmentBuilder& NGFragmentBuilder::SetWritingMode(
    NGWritingMode writing_mode) {
  writing_mode_ = writing_mode;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetDirection(TextDirection direction) {
  direction_ = direction;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetSize(const NGLogicalSize& size) {
  size_ = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockSize(LayoutUnit size) {
  size_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetOverflowSize(
    const NGLogicalSize& size) {
  overflow_ = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBlockOverflow(LayoutUnit size) {
  overflow_.block_size = size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddChild(
    RefPtr<NGLayoutResult> child,
    const NGLogicalOffset& child_offset) {
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentBox)
      << "Only box fragments can have children";

  // Collect child's out of flow descendants.
  const Vector<NGStaticPosition>& oof_positions = child->OutOfFlowPositions();
  size_t oof_index = 0;
  for (auto& oof_node : child->OutOfFlowDescendants()) {
    NGStaticPosition oof_position = oof_positions[oof_index++];
    out_of_flow_descendant_candidates_.insert(oof_node);
    out_of_flow_candidate_placements_.push_back(
        OutOfFlowPlacement{child_offset, oof_position});
  }

  return AddChild(child->PhysicalFragment(), child_offset);
}

NGFragmentBuilder& NGFragmentBuilder::AddChild(
    RefPtr<NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentBox)
      << "Only box fragments can have children";

  switch (child->Type()) {
    case NGPhysicalBoxFragment::kFragmentBox:
      // Update if we have fragmented in this flow.
      did_break_ |= !child->BreakToken()->IsFinished();
      child_break_tokens_.push_back(child->BreakToken());
      break;
    case NGPhysicalBoxFragment::kFragmentLineBox:
      // NGInlineNode produces multiple line boxes in an anonymous box. Only
      // the last break token is needed to be reported to the parent.
      DCHECK(child->BreakToken());
      DCHECK_EQ(child->BreakToken()->InputNode(), node_);
      last_inline_break_token_ =
          child->BreakToken()->IsFinished() ? nullptr : child->BreakToken();
      break;
    case NGPhysicalBoxFragment::kFragmentText:
      DCHECK(!child->BreakToken());
      break;
    default:
      NOTREACHED();
      break;
  }

  children_.push_back(std::move(child));
  offsets_.push_back(child_offset);

  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddPositionedFloat(
    NGPositionedFloat positioned_float) {
  did_break_ |= !positioned_float.fragment->BreakToken()->IsFinished();
  child_break_tokens_.push_back(positioned_float.fragment->BreakToken());
  positioned_floats_.push_back(positioned_float);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetBfcOffset(
    const NGLogicalOffset& offset) {
  bfc_offset_ = offset;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode* child,
    NGLogicalOffset child_offset) {
  out_of_flow_descendant_candidates_.insert(child);
  NGStaticPosition child_position =
      NGStaticPosition::Create(writing_mode_, direction_, NGPhysicalOffset());
  out_of_flow_candidate_placements_.push_back(
      OutOfFlowPlacement{child_offset, child_position});
  child->SaveStaticOffsetForLegacy(child_offset);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddUnpositionedFloat(
    RefPtr<NGUnpositionedFloat> unpositioned_float) {
  unpositioned_floats_.push_back(std::move(unpositioned_float));
  return *this;
}

void NGFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    WeakBoxList* descendants,
    Vector<NGStaticPosition>* descendant_positions) {
  DCHECK(descendants->IsEmpty());
  DCHECK(descendant_positions->IsEmpty());

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
    descendants->insert(oof_node);
    descendant_positions->push_back(builder_relative_position);
  }
  out_of_flow_descendant_candidates_.clear();
  out_of_flow_candidate_placements_.clear();
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowDescendant(
    NGBlockNode* descendant,
    const NGStaticPosition& position) {
  out_of_flow_descendants_.insert(descendant);
  out_of_flow_positions_.push_back(position);
  return *this;
}

RefPtr<NGLayoutResult> NGFragmentBuilder::ToBoxFragment() {
  DCHECK_EQ(type_, NGPhysicalFragment::kFragmentBox);
  DCHECK_EQ(offsets_.size(), children_.size());

  NGPhysicalSize physical_size = size_.ConvertToPhysical(writing_mode_);

  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].Get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        writing_mode_, direction_, physical_size, child->Size()));
  }

  RefPtr<NGBreakToken> break_token;
  if (last_inline_break_token_) {
    DCHECK(!last_inline_break_token_->IsFinished());
    child_break_tokens_.push_back(std::move(last_inline_break_token_));
    did_break_ = true;
  }
  if (did_break_) {
    break_token = NGBlockBreakToken::Create(node_.Get(), used_block_size_,
                                            child_break_tokens_);
  } else {
    break_token = NGBlockBreakToken::Create(node_.Get());
  }

  for (auto& positioned_float : positioned_floats_) {
    NGPhysicalFragment* floating_fragment = positioned_float.fragment.Get();
    floating_fragment->SetOffset(
        positioned_float.logical_offset.ConvertToPhysical(
            writing_mode_, direction_, physical_size,
            floating_fragment->Size()));
  }

  RefPtr<NGPhysicalBoxFragment> fragment = AdoptRef(new NGPhysicalBoxFragment(
      node_->GetLayoutObject(), physical_size,
      overflow_.ConvertToPhysical(writing_mode_), children_, positioned_floats_,
      bfc_offset_, end_margin_strut_, std::move(break_token)));

  return AdoptRef(
      new NGLayoutResult(std::move(fragment), out_of_flow_descendants_,
                         out_of_flow_positions_, unpositioned_floats_));
}

}  // namespace blink
