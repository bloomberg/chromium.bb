// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_fragment_builder.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_exclusion_space.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_positioned_float.h"

namespace blink {

NGFragmentBuilder::NGFragmentBuilder(NGLayoutInputNode node,
                                     scoped_refptr<const ComputedStyle> style,
                                     NGWritingMode writing_mode,
                                     TextDirection direction)
    : NGContainerFragmentBuilder(style, writing_mode, direction),
      node_(node),
      layout_object_(node.GetLayoutObject()),
      box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
      did_break_(false) {}

NGFragmentBuilder::NGFragmentBuilder(LayoutObject* layout_object,
                                     scoped_refptr<const ComputedStyle> style,
                                     NGWritingMode writing_mode,
                                     TextDirection direction)
    : NGContainerFragmentBuilder(style, writing_mode, direction),
      node_(nullptr),
      layout_object_(layout_object),
      box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
      did_break_(false) {}

NGFragmentBuilder::~NGFragmentBuilder() {}

NGFragmentBuilder& NGFragmentBuilder::SetBlockSize(LayoutUnit block_size) {
  block_size_ = block_size;
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::SetIntrinsicBlockSize(
    LayoutUnit intrinsic_block_size) {
  intrinsic_block_size_ = intrinsic_block_size;
  return *this;
}

NGContainerFragmentBuilder& NGFragmentBuilder::AddChild(
    scoped_refptr<NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  switch (child->Type()) {
    case NGPhysicalBoxFragment::kFragmentBox:
      if (child->BreakToken())
        child_break_tokens_.push_back(child->BreakToken());
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

  return NGContainerFragmentBuilder::AddChild(std::move(child), child_offset);
}

NGFragmentBuilder& NGFragmentBuilder::AddBreakBeforeChild(
    NGLayoutInputNode child) {
  // TODO(mstensho): Come up with a more intuitive way of creating an unfinished
  // break token. We currently need to pass a Vector here, just to end up in the
  // right NGBlockBreakToken constructor - the one that sets the token as
  // unfinished.
  Vector<scoped_refptr<NGBreakToken>> dummy;
  auto token = NGBlockBreakToken::Create(child, LayoutUnit(), dummy);
  child_break_tokens_.push_back(token);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::PropagateBreak(
    scoped_refptr<NGLayoutResult> child_layout_result) {
  if (!did_break_)
    return PropagateBreak(child_layout_result->PhysicalFragment());
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::PropagateBreak(
    scoped_refptr<NGPhysicalFragment> child_fragment) {
  if (!did_break_) {
    const auto* token = child_fragment->BreakToken();
    did_break_ = token && !token->IsFinished();
  }
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const NGLogicalOffset& child_offset) {
  DCHECK(child);

  oof_positioned_candidates_.push_back(NGOutOfFlowPositionedCandidate{
      NGOutOfFlowPositionedDescendant{
          child, NGStaticPosition::Create(WritingMode(), Direction(),
                                          NGPhysicalOffset())},
      child_offset});

  child.SaveStaticOffsetForLegacy(child_offset);
  return *this;
}

void NGFragmentBuilder::AddOutOfFlowLegacyCandidate(
    NGBlockNode node,
    const NGStaticPosition& static_position) {
  DCHECK_GE(inline_size_, LayoutUnit());
  DCHECK_GE(block_size_, LayoutUnit());

  NGOutOfFlowPositionedDescendant descendant{node, static_position};
  // Need 0,0 physical coordinates as child offset. Because offset
  // is stored as logical, must convert physical 0,0 to logical.
  NGLogicalOffset zero_offset;
  switch (WritingMode()) {
    case kHorizontalTopBottom:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(inline_size_, LayoutUnit());
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset(LayoutUnit(), block_size_);
      else
        zero_offset = NGLogicalOffset(inline_size_, block_size_);
      break;
    case kVerticalLeftRight:
    case kSidewaysLeftRight:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(inline_size_, LayoutUnit());
      break;
  }
  oof_positioned_candidates_.push_back(
      NGOutOfFlowPositionedCandidate{descendant, zero_offset});
}

void NGFragmentBuilder::GetAndClearOutOfFlowDescendantCandidates(
    Vector<NGOutOfFlowPositionedDescendant>* descendant_candidates) {
  DCHECK(descendant_candidates->IsEmpty());

  descendant_candidates->ReserveCapacity(oof_positioned_candidates_.size());

  DCHECK_GE(inline_size_, LayoutUnit());
  DCHECK_GE(block_size_, LayoutUnit());
  NGPhysicalSize builder_physical_size{Size().ConvertToPhysical(WritingMode())};

  for (NGOutOfFlowPositionedCandidate& candidate : oof_positioned_candidates_) {
    NGPhysicalOffset child_offset = candidate.child_offset.ConvertToPhysical(
        WritingMode(), Direction(), builder_physical_size, NGPhysicalSize());

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

NGPhysicalFragment::NGBoxType NGFragmentBuilder::BoxType() const {
  if (box_type_ != NGPhysicalFragment::NGBoxType::kNormalBox)
    return box_type_;
  // When implicit, compute from LayoutObject.
  if (!layout_object_ || layout_object_->Style() != &Style())
    return NGPhysicalFragment::NGBoxType::kAnonymousBox;
  if (layout_object_->IsFloating())
    return NGPhysicalFragment::NGBoxType::kFloating;
  if (layout_object_->IsOutOfFlowPositioned())
    return NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned;
  if (layout_object_->IsAtomicInlineLevel())
    return NGPhysicalFragment::NGBoxType::kInlineBlock;
  return NGPhysicalFragment::NGBoxType::kNormalBox;
}

NGFragmentBuilder& NGFragmentBuilder::SetBoxType(
    NGPhysicalFragment::NGBoxType box_type) {
  box_type_ = box_type;
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

scoped_refptr<NGLayoutResult> NGFragmentBuilder::ToBoxFragment() {
  DCHECK_EQ(offsets_.size(), children_.size());

  NGPhysicalSize physical_size = Size().ConvertToPhysical(WritingMode());

  NGPhysicalOffsetRect contents_visual_rect({}, physical_size);
  for (size_t i = 0; i < children_.size(); ++i) {
    NGPhysicalFragment* child = children_[i].get();
    child->SetOffset(offsets_[i].ConvertToPhysical(
        WritingMode(), Direction(), physical_size, child->Size()));
    child->PropagateContentsVisualRect(&contents_visual_rect);
  }

  scoped_refptr<NGBreakToken> break_token;
  if (node_) {
    if (last_inline_break_token_) {
      DCHECK(!last_inline_break_token_->IsFinished());
      child_break_tokens_.push_back(std::move(last_inline_break_token_));
    }
    if (did_break_) {
      break_token = NGBlockBreakToken::Create(node_, used_block_size_,
                                              child_break_tokens_);
    } else {
      break_token = NGBlockBreakToken::Create(node_, used_block_size_);
    }
  }

  scoped_refptr<NGPhysicalBoxFragment> fragment =
      WTF::AdoptRef(new NGPhysicalBoxFragment(
          layout_object_, Style(), physical_size, contents_visual_rect,
          children_, baselines_, BoxType(),
          border_edges_.ToPhysical(WritingMode()), std::move(break_token)));

  Vector<NGPositionedFloat> positioned_floats;

  return WTF::AdoptRef(new NGLayoutResult(
      std::move(fragment), oof_positioned_descendants_, positioned_floats,
      unpositioned_floats_, std::move(exclusion_space_), bfc_offset_,
      end_margin_strut_, intrinsic_block_size_, NGLayoutResult::kSuccess));
}

scoped_refptr<NGLayoutResult> NGFragmentBuilder::Abort(
    NGLayoutResult::NGLayoutResultStatus status) {
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants;
  Vector<NGPositionedFloat> positioned_floats;
  return WTF::AdoptRef(
      new NGLayoutResult(nullptr, oof_positioned_descendants, positioned_floats,
                         unpositioned_floats_, nullptr, bfc_offset_,
                         end_margin_strut_, LayoutUnit(), status));
}

}  // namespace blink
