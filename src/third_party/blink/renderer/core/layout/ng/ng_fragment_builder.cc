// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

namespace {

NGPhysicalFragment::NGBoxType BoxTypeFromLayoutObject(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  if (layout_object->IsFloating())
    return NGPhysicalFragment::NGBoxType::kFloating;
  if (layout_object->IsOutOfFlowPositioned())
    return NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned;
  if (layout_object->IsAtomicInlineLevel())
    return NGPhysicalFragment::NGBoxType::kAtomicInline;
  if (layout_object->IsInline())
    return NGPhysicalFragment::NGBoxType::kInlineBox;
  if (layout_object->IsLayoutBlock() &&
      ToLayoutBlock(layout_object)->CreatesNewFormattingContext())
    return NGPhysicalFragment::NGBoxType::kBlockFlowRoot;
  return NGPhysicalFragment::NGBoxType::kNormalBox;
}

}  // namespace

NGFragmentBuilder::NGFragmentBuilder(NGLayoutInputNode node,
                                     scoped_refptr<const ComputedStyle> style,
                                     WritingMode writing_mode,
                                     TextDirection direction)
    : NGContainerFragmentBuilder(std::move(style), writing_mode, direction),
      node_(node),
      layout_object_(node.GetLayoutBox()),
      box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
      is_old_layout_root_(false),
      did_break_(false) {}

NGFragmentBuilder::NGFragmentBuilder(LayoutObject* layout_object,
                                     scoped_refptr<const ComputedStyle> style,
                                     WritingMode writing_mode,
                                     TextDirection direction)
    : NGContainerFragmentBuilder(std::move(style), writing_mode, direction),
      node_(nullptr),
      layout_object_(layout_object),
      box_type_(NGPhysicalFragment::NGBoxType::kNormalBox),
      is_old_layout_root_(false),
      did_break_(false) {}

NGFragmentBuilder::~NGFragmentBuilder() = default;

NGContainerFragmentBuilder& NGFragmentBuilder::AddChild(
    scoped_refptr<const NGPhysicalFragment> child,
    const NGLogicalOffset& child_offset) {
  switch (child->Type()) {
    case NGPhysicalBoxFragment::kFragmentBox:
    case NGPhysicalBoxFragment::kFragmentRenderedLegend:
      if (child->BreakToken())
        child_break_tokens_.push_back(child->BreakToken());
      break;
    case NGPhysicalBoxFragment::kFragmentLineBox:
      // NGInlineNode produces multiple line boxes in an anonymous box. We won't
      // know up front which line box to insert a fragment break before (due to
      // widows), so keep them all until we know.
      DCHECK(child->BreakToken());
      DCHECK(child->BreakToken()->InputNode() == node_);
      inline_break_tokens_.push_back(child->BreakToken());
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

void NGFragmentBuilder::RemoveChildren() {
  child_break_tokens_.clear();
  inline_break_tokens_.clear();
  children_.clear();
  offsets_.clear();
}

NGFragmentBuilder& NGFragmentBuilder::AddBreakBeforeChild(
    NGLayoutInputNode child) {
  if (child.IsInline()) {
    if (inline_break_tokens_.IsEmpty()) {
      // In some cases we may want to break before the first line, as a last
      // resort. We need a break token for that as well, so that the machinery
      // will understand that we should resume at the beginning of the inline
      // formatting context, rather than concluding that we're done with the
      // whole thing.
      inline_break_tokens_.push_back(NGInlineBreakToken::Create(
          ToNGInlineNode(child), nullptr, 0, 0, NGInlineBreakToken::kDefault));
    }
    return *this;
  }
  auto token = NGBlockBreakToken::CreateBreakBefore(child);
  child_break_tokens_.push_back(token);
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::AddBreakBeforeLine(int line_number) {
  DCHECK_GT(line_number, 0);
  DCHECK_LE(unsigned(line_number), inline_break_tokens_.size());
  int lines_to_remove = inline_break_tokens_.size() - line_number;
  if (lines_to_remove > 0) {
    // Remove widows that should be pushed to the next fragment. We'll also
    // remove all other child fragments than line boxes (typically floats) that
    // come after the first line that's moved, as those also have to be re-laid
    // out in the next fragment.
    inline_break_tokens_.resize(line_number);
    DCHECK_GT(children_.size(), 0UL);
    for (int i = children_.size() - 1; i >= 0; i--) {
      DCHECK_NE(i, 0);
      if (!children_[i]->IsLineBox())
        continue;
      if (!--lines_to_remove) {
        // This is the first line that is going to the next fragment. Remove it,
        // and everything after it.
        children_.resize(i);
        offsets_.resize(i);
        break;
      }
    }
  }

  // We need to resume at the right inline location in the next fragment, but
  // broken floats, which are resumed and positioned by the parent block layout
  // algorithm, need to be ignored by the inline layout algorithm.
  ToNGInlineBreakToken(inline_break_tokens_.back().get())->SetIgnoreFloats();
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::PropagateBreak(
    scoped_refptr<NGLayoutResult> child_layout_result) {
  if (!did_break_)
    PropagateBreak(child_layout_result->PhysicalFragment());
  if (child_layout_result->HasForcedBreak())
    SetHasForcedBreak();
  else
    PropagateSpaceShortage(child_layout_result->MinimalSpaceShortage());
  return *this;
}

NGFragmentBuilder& NGFragmentBuilder::PropagateBreak(
    scoped_refptr<const NGPhysicalFragment> child_fragment) {
  if (!did_break_) {
    const auto* token = child_fragment->BreakToken();
    did_break_ = token && !token->IsFinished();
  }
  return *this;
}

void NGFragmentBuilder::AddOutOfFlowLegacyCandidate(
    NGBlockNode node,
    const NGStaticPosition& static_position,
    LayoutObject* inline_container) {
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());

  NGOutOfFlowPositionedDescendant descendant{node, static_position,
                                             inline_container};
  // Need 0,0 physical coordinates as child offset. Because offset
  // is stored as logical, must convert physical 0,0 to logical.
  NGLogicalOffset zero_offset;
  switch (GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(InlineSize(), LayoutUnit());
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset(LayoutUnit(), BlockSize());
      else
        zero_offset = NGLogicalOffset(InlineSize(), BlockSize());
      break;
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysLr:
      if (IsLtr(Direction()))
        zero_offset = NGLogicalOffset();
      else
        zero_offset = NGLogicalOffset(InlineSize(), LayoutUnit());
      break;
  }
  oof_positioned_candidates_.push_back(
      NGOutOfFlowPositionedCandidate{descendant, zero_offset});
}

NGPhysicalFragment::NGBoxType NGFragmentBuilder::BoxType() const {
  if (box_type_ != NGPhysicalFragment::NGBoxType::kNormalBox)
    return box_type_;

  // When implicit, compute from LayoutObject.
  return BoxTypeFromLayoutObject(layout_object_);
}

void NGFragmentBuilder::AddBaseline(NGBaselineRequest request,
                                    LayoutUnit offset) {
#if DCHECK_IS_ON()
  for (const auto& baseline : baselines_)
    DCHECK(baseline.request != request);
#endif
  baselines_.push_back(NGBaseline{request, offset});
}

EBreakBetween NGFragmentBuilder::JoinedBreakBetweenValue(
    EBreakBetween break_before) const {
  return JoinFragmentainerBreakValues(previous_break_after_, break_before);
}

scoped_refptr<NGLayoutResult> NGFragmentBuilder::ToBoxFragment(
    WritingMode block_or_line_writing_mode) {
  DCHECK_EQ(offsets_.size(), children_.size());

  NGPhysicalSize physical_size = Size().ConvertToPhysical(GetWritingMode());

  DCHECK_EQ(children_.size(), offsets_.size());
  for (size_t i = 0; i < children_.size(); i++) {
    auto& child = children_[i];
    child.offset_ = offsets_[i].ConvertToPhysical(
        block_or_line_writing_mode, Direction(), physical_size, child->Size());
  }

  scoped_refptr<NGBreakToken> break_token;
  bool is_rendered_legend = false;
  if (node_) {
    is_rendered_legend = node_.IsRenderedLegend();
    if (!inline_break_tokens_.IsEmpty()) {
      if (auto token = inline_break_tokens_.back()) {
        if (!token->IsFinished())
          child_break_tokens_.push_back(std::move(token));
      }
    }
    if (did_break_) {
      break_token = NGBlockBreakToken::Create(
          node_, used_block_size_, child_break_tokens_, has_last_resort_break_);
    } else {
      break_token = NGBlockBreakToken::Create(node_, used_block_size_,
                                              has_last_resort_break_);
    }
  }

  // Because this vector will be long-lived, make sure to not waste space.
  // (We reserve an initial capacity when adding the first child)
  if (children_.size())
    children_.ShrinkToReasonableCapacity();
  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      base::AdoptRef(new NGPhysicalBoxFragment(
          layout_object_, Style(), style_variant_, physical_size, children_,
          borders_.ConvertToPhysical(GetWritingMode(), Direction()),
          padding_.ConvertToPhysical(GetWritingMode(), Direction()), baselines_,
          BoxType(), is_fieldset_container_, is_rendered_legend,
          is_old_layout_root_, border_edges_.ToPhysical(GetWritingMode()),
          std::move(break_token)));

  Vector<NGPositionedFloat> positioned_floats;

  return base::AdoptRef(new NGLayoutResult(
      std::move(fragment), std::move(oof_positioned_descendants_),
      std::move(positioned_floats), unpositioned_list_marker_,
      std::move(exclusion_space_), bfc_line_offset_, bfc_block_offset_,
      end_margin_strut_, intrinsic_block_size_, minimal_space_shortage_,
      initial_break_before_, previous_break_after_, has_forced_break_,
      is_pushed_by_floats_, adjoining_floats_, NGLayoutResult::kSuccess));
}

scoped_refptr<NGLayoutResult> NGFragmentBuilder::Abort(
    NGLayoutResult::NGLayoutResultStatus status) {
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants;
  Vector<NGPositionedFloat> positioned_floats;
  return base::AdoptRef(new NGLayoutResult(
      nullptr, std::move(oof_positioned_descendants),
      std::move(positioned_floats), NGUnpositionedListMarker(),
      NGExclusionSpace(), bfc_line_offset_, bfc_block_offset_,
      end_margin_strut_, LayoutUnit(), LayoutUnit(), EBreakBetween::kAuto,
      EBreakBetween::kAuto, false, false, kFloatTypeNone, status));
}

// Finds FragmentPairs that define inline containing blocks.
// inline_container_fragments is a map whose keys specify which
// inline containing blocks are required.
// Not finding a required block is an unexpected behavior (DCHECK).
void NGFragmentBuilder::ComputeInlineContainerFragments(
    HashMap<const LayoutObject*, FragmentPair>* inline_container_fragments,
    NGLogicalSize* container_size) {
  // This function has detailed knowledge of inline fragment tree structure,
  // and will break if this changes.
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());
  *container_size = Size();

  for (size_t i = 0; i < children_.size(); i++) {
    if (children_[i]->IsLineBox()) {
      const NGPhysicalLineBoxFragment* linebox =
          ToNGPhysicalLineBoxFragment(children_[i].get());
      for (auto& descendant :
           NGInlineFragmentTraversal::DescendantsOf(*linebox)) {
        LayoutObject* key = {};
        if (descendant.fragment->IsText()) {
          key = descendant.fragment->GetLayoutObject();
          DCHECK(key);
          key = key->Parent();
          DCHECK(key);
        } else if (descendant.fragment->IsBox()) {
          key = descendant.fragment->GetLayoutObject();
        }
        if (key && inline_container_fragments->Contains(key)) {
          NGFragmentBuilder::FragmentPair value =
              inline_container_fragments->at(key);
          if (!value.start_fragment) {
            value.start_fragment = descendant.fragment.get();
            value.start_fragment_union_rect.offset =
                descendant.offset_to_container_box;
            value.start_fragment_union_rect =
                NGPhysicalOffsetRect(descendant.offset_to_container_box,
                                     value.start_fragment->Size());
            value.start_linebox_fragment = linebox;
            value.start_linebox_offset = offsets_.at(i);
          }
          if (!value.end_fragment || value.end_linebox_fragment != linebox) {
            value.end_fragment = descendant.fragment.get();
            value.end_fragment_union_rect = NGPhysicalOffsetRect(
                descendant.offset_to_container_box, value.end_fragment->Size());
            value.end_linebox_fragment = linebox;
            value.end_linebox_offset = offsets_.at(i);
          }
          // Extend the union size
          if (value.start_linebox_fragment == linebox) {
            // std::max because initial box might have larger extent than its
            // descendants.
            value.start_fragment_union_rect.size.width =
                std::max(descendant.offset_to_container_box.left +
                             descendant.fragment->Size().width -
                             value.start_fragment_union_rect.offset.left,
                         value.start_fragment_union_rect.size.width);
            value.start_fragment_union_rect.size.height =
                std::max(descendant.offset_to_container_box.top +
                             descendant.fragment->Size().height -
                             value.start_fragment_union_rect.offset.top,
                         value.start_fragment_union_rect.size.height);
          }
          if (value.end_linebox_fragment == linebox) {
            value.end_fragment_union_rect.size.width =
                std::max(descendant.offset_to_container_box.left +
                             descendant.fragment->Size().width -
                             value.start_fragment_union_rect.offset.left,
                         value.end_fragment_union_rect.size.width);
            value.end_fragment_union_rect.size.height =
                std::max(descendant.offset_to_container_box.top +
                             descendant.fragment->Size().height -
                             value.start_fragment_union_rect.offset.top,
                         value.end_fragment_union_rect.size.height);
          }
          inline_container_fragments->Set(key, value);
        }
      }
    }
  }
}

}  // namespace blink
