// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
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
                                     NGLayoutInputNode node)
    : type_(type),
      writing_mode_(kHorizontalTopBottom),
      direction_(TextDirection::kLtr),
      node_(node),
      layout_object_(node.GetLayoutObject()),
      did_break_(false) {}

NGFragmentBuilder::NGFragmentBuilder(NGPhysicalFragment::NGFragmentType type,
                                     LayoutObject* layout_object)
    : type_(type),
      writing_mode_(kHorizontalTopBottom),
      direction_(TextDirection::kLtr),
      node_(nullptr),
      layout_object_(layout_object),
      did_break_(false) {}

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
  for (const NGOutOfFlowPositionedDescendant& descendant :
       child->OutOfFlowPositionedDescendants()) {
    oof_positioned_candidates_.push_back(
        NGOutOfFlowPositionedCandidate{descendant, child_offset});
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
      if (child->BreakToken()) {
        did_break_ |= !child->BreakToken()->IsFinished();
        child_break_tokens_.push_back(child->BreakToken());
      }
      break;
    case NGPhysicalBoxFragment::kFragmentLineBox:
      // NGInlineNode produces multiple line boxes in an anonymous box. Only
      // the last break token is needed to be reported to the parent.
      DCHECK(child->BreakToken());
      DCHECK(child->BreakToken()->InputNode() == node_);
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
    NGBlockNode child,
    const NGLogicalOffset& child_offset) {
  DCHECK(child);

  oof_positioned_candidates_.push_back(NGOutOfFlowPositionedCandidate{
      NGOutOfFlowPositionedDescendant{
          child, NGStaticPosition::Create(writing_mode_, direction_,
                                          NGPhysicalOffset())},
      child_offset});

  child.SaveStaticOffsetForLegacy(child_offset);
  return *this;
}

void NGFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates) {
  DCHECK(descendant_candidates->IsEmpty());

  descendant_candidates->ReserveCapacity(oof_positioned_candidates_.size());

  DCHECK_GE(size_.inline_size, LayoutUnit());
  DCHECK_GE(size_.block_size, LayoutUnit());
  NGPhysicalSize builder_physical_size{size_.ConvertToPhysical(writing_mode_)};

  for (NGOutOfFlowPositionedCandidate& candidate : oof_positioned_candidates_) {
    NGPhysicalOffset child_offset = candidate.child_offset.ConvertToPhysical(
        writing_mode_, direction_, builder_physical_size, NGPhysicalSize());

    NGStaticPosition builder_relative_position;
    builder_relative_position.type = candidate.descendant.static_position.type;
    builder_relative_position.offset =
        child_offset + candidate.descendant.static_position.offset;

    descendant_candidates->push_back(NGOutOfFlowPositionedDescendant{
        candidate.descendant.node, builder_relative_position});
  }

  // Clear our current canidate list. This may get modified again if the
  // current fragment is a containing block, and AddChild is called with a
  // descendant from this list.
  //
  // The descendant may be a "position: absolute" which contains a "position:
  // fixed" for example. (This fragment isn't the containing block for the
  // fixed descendant).
  oof_positioned_candidates_.clear();
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowDescendant(
    NGOutOfFlowPositionedDescendant descendant) {
  oof_positioned_descendants_.push_back(descendant);
  return *this;
}

void NGFragmentBuilder::AddBaseline(NGBaselineRequest request,
                                    LayoutUnit offset) {
#if DCHECK_IS_ON()
  for (const auto& baseline : baselines_)
    DCHECK(baseline.request != request);
#endif
  baselines_.push_back(NGBaseline{request, offset});
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
  if (node_) {
    if (last_inline_break_token_) {
      DCHECK(!last_inline_break_token_->IsFinished());
      child_break_tokens_.push_back(std::move(last_inline_break_token_));
      did_break_ = true;
    }
    if (did_break_) {
      break_token = NGBlockBreakToken::Create(node_, used_block_size_,
                                              child_break_tokens_);
    } else {
      break_token = NGBlockBreakToken::Create(node_);
    }
  }

  for (auto& positioned_float : positioned_floats_) {
    NGPhysicalFragment* floating_fragment = positioned_float.fragment.Get();
    floating_fragment->SetOffset(
        positioned_float.logical_offset.ConvertToPhysical(
            writing_mode_, direction_, physical_size,
            floating_fragment->Size()));
  }

  RefPtr<NGPhysicalBoxFragment> fragment = AdoptRef(new NGPhysicalBoxFragment(
      layout_object_, physical_size, overflow_.ConvertToPhysical(writing_mode_),
      children_, positioned_floats_, baselines_,
      border_edges_.ToPhysical(writing_mode_), std::move(break_token)));

  return AdoptRef(new NGLayoutResult(
      std::move(fragment), oof_positioned_descendants_, unpositioned_floats_,
      bfc_offset_, end_margin_strut_, NGLayoutResult::kSuccess));
}

RefPtr<NGLayoutResult> NGFragmentBuilder::Abort(
    NGLayoutResult::NGLayoutResultStatus status) {
  return AdoptRef(new NGLayoutResult(
      nullptr, Vector<NGOutOfFlowPositionedDescendant>(), unpositioned_floats_,
      bfc_offset_, end_margin_strut_, status));
}

}  // namespace blink
