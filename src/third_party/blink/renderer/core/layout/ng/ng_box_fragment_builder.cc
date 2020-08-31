// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

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
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// std::pair.first points to the start linebox fragment.
// std::pair.second points to the end linebox fragment.
using LineBoxPair = std::pair<const NGPhysicalLineBoxFragment*,
                              const NGPhysicalLineBoxFragment*>;

void GatherInlineContainerFragmentsFromLinebox(
    NGBoxFragmentBuilder::InlineContainingBlockMap* inline_containing_block_map,
    HashMap<const LayoutObject*, LineBoxPair>* containing_linebox_map,
    const NGPhysicalLineBoxFragment& linebox,
    const PhysicalOffset linebox_offset) {
  for (const auto& descendant :
       NGInlineFragmentTraversal::DescendantsOf(linebox)) {
    if (!descendant.fragment->IsBox())
      continue;
    const LayoutObject* key = descendant.fragment->GetLayoutObject();
    // Key for inline is the continuation root if it exists.
    if (key->IsLayoutInline() && key->GetNode())
      key = key->ContinuationRoot();
    auto it = inline_containing_block_map->find(key);
    if (it == inline_containing_block_map->end()) {
      // Default case, not one of the blocks we are looking for.
      continue;
    }
    base::Optional<NGBoxFragmentBuilder::InlineContainingBlockGeometry>&
        containing_block_geometry = it->value;
    LineBoxPair& containing_lineboxes =
        containing_linebox_map->insert(key, LineBoxPair{nullptr, nullptr})
            .stored_value->value;
    DCHECK(containing_block_geometry.has_value() ||
           !containing_lineboxes.first);

    // |DescendantsOf| returns the offset from the given fragment. Since
    // we give it the line box, need to add the |linebox_offset|.
    PhysicalRect fragment_rect(
        linebox_offset + descendant.offset_to_container_box,
        descendant.fragment->Size());
    if (containing_lineboxes.first == &linebox) {
      containing_block_geometry->start_fragment_union_rect.Unite(fragment_rect);
    } else if (!containing_lineboxes.first) {
      containing_lineboxes.first = &linebox;
      containing_block_geometry =
          NGBoxFragmentBuilder::InlineContainingBlockGeometry{fragment_rect,
                                                              PhysicalRect()};
    }
    // Skip fragments within an empty line boxes for the end fragment.
    if (containing_lineboxes.second == &linebox) {
      containing_block_geometry->end_fragment_union_rect.Unite(fragment_rect);
    } else if (!containing_lineboxes.second || !linebox.IsEmptyLineBox()) {
      containing_lineboxes.second = &linebox;
      containing_block_geometry->end_fragment_union_rect = fragment_rect;
    }
  }
}

template <class Items>
void GatherInlineContainerFragmentsFromItems(
    const Items& items,
    const PhysicalOffset& box_offset,
    NGBoxFragmentBuilder::InlineContainingBlockMap* inline_containing_block_map,
    HashMap<const LayoutObject*, LineBoxPair>* containing_linebox_map) {
  const NGPhysicalLineBoxFragment* linebox = nullptr;
  for (const auto& item : items) {
    // Track the current linebox.
    if (const NGPhysicalLineBoxFragment* current_linebox =
            item->LineBoxFragment()) {
      linebox = current_linebox;
      continue;
    }

    // We only care about inlines which have generated a box fragment.
    const NGPhysicalBoxFragment* box = item->BoxFragment();
    if (!box)
      continue;

    // The key for the inline is the continuation root if it exists.
    const LayoutObject* key = box->GetLayoutObject();
    if (key->IsLayoutInline() && key->GetNode())
      key = key->ContinuationRoot();

    // See if we need the containing block information for this inline.
    auto it = inline_containing_block_map->find(key);
    if (it == inline_containing_block_map->end())
      continue;

    base::Optional<NGBoxFragmentBuilder::InlineContainingBlockGeometry>&
        containing_block_geometry = it->value;
    LineBoxPair& containing_lineboxes =
        containing_linebox_map->insert(key, LineBoxPair{nullptr, nullptr})
            .stored_value->value;
    DCHECK(containing_block_geometry.has_value() ||
           !containing_lineboxes.first);

    PhysicalRect fragment_rect = item->RectInContainerBlock();
    fragment_rect.offset += box_offset;
    if (containing_lineboxes.first == linebox) {
      // Unite the start rect with the fragment's rect.
      containing_block_geometry->start_fragment_union_rect.Unite(fragment_rect);
    } else if (!containing_lineboxes.first) {
      DCHECK(!containing_lineboxes.second);
      // This is the first linebox we've encountered, initialize the containing
      // block geometry.
      containing_lineboxes.first = linebox;
      containing_lineboxes.second = linebox;
      containing_block_geometry =
          NGBoxFragmentBuilder::InlineContainingBlockGeometry{fragment_rect,
                                                              fragment_rect};
    }

    if (containing_lineboxes.second == linebox) {
      // Unite the end rect with the fragment's rect.
      containing_block_geometry->end_fragment_union_rect.Unite(fragment_rect);
    } else if (!linebox->IsEmptyLineBox()) {
      // We've found a new "end" linebox,  update the containing block geometry.
      containing_lineboxes.second = linebox;
      containing_block_geometry->end_fragment_union_rect = fragment_rect;
    }
  }
}

}  // namespace

void NGBoxFragmentBuilder::AddBreakBeforeChild(
    NGLayoutInputNode child,
    base::Optional<NGBreakAppeal> appeal,
    bool is_forced_break) {
  if (appeal)
    break_appeal_ = *appeal;
  if (is_forced_break) {
    SetHasForcedBreak();
    // A forced break is considered to always have perfect appeal; they should
    // never be weighed against other potential breakpoints.
    DCHECK(!appeal || *appeal == kBreakAppealPerfect);
  }

  DCHECK(has_block_fragmentation_);
  SetDidBreak();
  if (auto* child_inline_node = DynamicTo<NGInlineNode>(child)) {
    if (inline_break_tokens_.IsEmpty()) {
      // In some cases we may want to break before the first line, as a last
      // resort. We need a break token for that as well, so that the machinery
      // will understand that we should resume at the beginning of the inline
      // formatting context, rather than concluding that we're done with the
      // whole thing.
      inline_break_tokens_.push_back(NGInlineBreakToken::Create(
          *child_inline_node, /* style */ nullptr, /* item_index */ 0,
          /* text_offset */ 0, NGInlineBreakToken::kDefault));
    }
    return;
  }
  auto token = NGBlockBreakToken::CreateBreakBefore(child, is_forced_break);
  child_break_tokens_.push_back(token);
}

void NGBoxFragmentBuilder::AddResult(const NGLayoutResult& child_layout_result,
                                     const LogicalOffset offset) {
  const auto& fragment = child_layout_result.PhysicalFragment();
  if (items_builder_) {
    if (const NGPhysicalLineBoxFragment* line =
            DynamicTo<NGPhysicalLineBoxFragment>(&fragment)) {
      items_builder_->AddLine(*line, offset);
      // TODO(kojii): We probably don't need to AddChild this line, but there
      // maybe OOF objects. Investigate how to handle them.
    }
  }
  AddChild(fragment, offset);
  if (fragment.IsBox())
    PropagateBreak(child_layout_result);
}

void NGBoxFragmentBuilder::AddBreakToken(
    scoped_refptr<const NGBreakToken> token) {
  DCHECK(token.get());
  child_break_tokens_.push_back(std::move(token));
}

void NGBoxFragmentBuilder::AddOutOfFlowLegacyCandidate(
    NGBlockNode node,
    const NGLogicalStaticPosition& static_position,
    const LayoutInline* inline_container) {
  oof_positioned_candidates_.emplace_back(
      node, static_position,
      inline_container ? ToLayoutInline(inline_container->ContinuationRoot())
                       : nullptr);
}

NGPhysicalFragment::NGBoxType NGBoxFragmentBuilder::BoxType() const {
  if (box_type_ != NGPhysicalFragment::NGBoxType::kNormalBox)
    return box_type_;

  // When implicit, compute from LayoutObject.
  DCHECK(layout_object_);
  if (layout_object_->IsFloating())
    return NGPhysicalFragment::NGBoxType::kFloating;
  if (layout_object_->IsOutOfFlowPositioned())
    return NGPhysicalFragment::NGBoxType::kOutOfFlowPositioned;
  if (layout_object_->IsRenderedLegend())
    return NGPhysicalFragment::NGBoxType::kRenderedLegend;
  if (layout_object_->IsInline()) {
    // Check |IsAtomicInlineLevel()| after |IsInline()| because |LayoutReplaced|
    // sets |IsAtomicInlineLevel()| even when it's block-level. crbug.com/567964
    if (layout_object_->IsAtomicInlineLevel())
      return NGPhysicalFragment::NGBoxType::kAtomicInline;
    return NGPhysicalFragment::NGBoxType::kInlineBox;
  }
  DCHECK(node_) << "Must call SetBoxType if there is no node";
  DCHECK_EQ(is_new_fc_, node_.CreatesNewFormattingContext())
      << "Forgot to call builder.SetIsNewFormattingContext";
  if (is_new_fc_)
    return NGPhysicalFragment::NGBoxType::kBlockFlowRoot;
  return NGPhysicalFragment::NGBoxType::kNormalBox;
}

EBreakBetween NGBoxFragmentBuilder::JoinedBreakBetweenValue(
    EBreakBetween break_before) const {
  return JoinFragmentainerBreakValues(previous_break_after_, break_before);
}

void NGBoxFragmentBuilder::PropagateBreak(
    const NGLayoutResult& child_layout_result) {
  if (LIKELY(!has_block_fragmentation_))
    return;
  if (!did_break_) {
    const auto* token = child_layout_result.PhysicalFragment().BreakToken();
    did_break_ = token && !token->IsFinished();
  }
  if (child_layout_result.HasForcedBreak()) {
    SetHasForcedBreak();
  } else if (IsInitialColumnBalancingPass()) {
    PropagateTallestUnbreakableBlockSize(
        child_layout_result.TallestUnbreakableBlockSize());
  } else {
    PropagateSpaceShortage(child_layout_result.MinimalSpaceShortage());
  }
}

scoped_refptr<const NGLayoutResult> NGBoxFragmentBuilder::ToBoxFragment(
    WritingMode block_or_line_writing_mode) {
#if DCHECK_IS_ON()
  if (ItemsBuilder()) {
    for (const ChildWithOffset& child : Children()) {
      DCHECK(child.fragment);
      const NGPhysicalFragment& fragment = *child.fragment;
      DCHECK(fragment.IsLineBox() ||
             // TODO(kojii): How to place floats and OOF is TBD.
             fragment.IsFloatingOrOutOfFlowPositioned());
    }
  }
#endif

  if (UNLIKELY(node_ && has_block_fragmentation_)) {
    if (!inline_break_tokens_.IsEmpty()) {
      if (auto token = inline_break_tokens_.back()) {
        if (!token->IsFinished())
          child_break_tokens_.push_back(std::move(token));
      }
    }
    if (did_break_) {
      break_token_ = NGBlockBreakToken::Create(
          node_, consumed_block_size_, sequence_number_, child_break_tokens_,
          break_appeal_, has_seen_all_children_);
    }
  }

  if (!has_floating_descendants_for_paint_ && items_builder_) {
    has_floating_descendants_for_paint_ =
        items_builder_->HasFloatingDescendantsForPaint();
  }

  scoped_refptr<const NGPhysicalBoxFragment> fragment =
      NGPhysicalBoxFragment::Create(this, block_or_line_writing_mode);
  fragment->CheckType();

  return base::AdoptRef(
      new NGLayoutResult(NGLayoutResult::NGBoxFragmentBuilderPassKey(),
                         std::move(fragment), this));
}

scoped_refptr<const NGLayoutResult> NGBoxFragmentBuilder::Abort(
    NGLayoutResult::EStatus status) {
  return base::AdoptRef(new NGLayoutResult(
      NGLayoutResult::NGBoxFragmentBuilderPassKey(), status, this));
}

LogicalOffset NGBoxFragmentBuilder::GetChildOffset(
    const LayoutObject* object) const {
  DCHECK(object);

  if (const NGFragmentItemsBuilder* items_builder = items_builder_) {
    if (auto offset = items_builder->LogicalOffsetFor(*object))
      return *offset;
    NOTREACHED();
    return LogicalOffset();
  }

  for (const auto& child : children_) {
    if (child.fragment->GetLayoutObject() == object)
      return child.offset;

    // TODO(layout-dev): ikilpatrick thinks we may need to traverse
    // further than the initial line-box children for a nested inline
    // container. We could not come up with a testcase, it would be
    // something with split inlines, and nested oof/fixed descendants maybe.
    if (child.fragment->IsLineBox()) {
      const auto& line_box_fragment =
          To<NGPhysicalLineBoxFragment>(*child.fragment);
      for (const auto& line_box_child : line_box_fragment.Children()) {
        if (line_box_child->GetLayoutObject() == object) {
          return child.offset + line_box_child.Offset().ConvertToLogical(
                                    GetWritingMode(), Direction(),
                                    line_box_fragment.Size(),
                                    line_box_child->Size());
        }
      }
    }
  }
  NOTREACHED();
  return LogicalOffset();
}

void NGBoxFragmentBuilder::ComputeInlineContainerGeometryFromFragmentTree(
    InlineContainingBlockMap* inline_containing_block_map) {
  if (inline_containing_block_map->IsEmpty())
    return;

  // This function has detailed knowledge of inline fragment tree structure,
  // and will break if this changes.
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());
#if DCHECK_IS_ON()
  // Make sure all entries are continuation root.
  for (const auto& entry : *inline_containing_block_map)
    DCHECK_EQ(entry.key, entry.key->ContinuationRoot());
#endif

  HashMap<const LayoutObject*, LineBoxPair> containing_linebox_map;
  for (const auto& child : children_) {
    if (child.fragment->IsLineBox()) {
      const auto& linebox = To<NGPhysicalLineBoxFragment>(*child.fragment);
      const PhysicalOffset linebox_offset = child.offset.ConvertToPhysical(
          GetWritingMode(), Direction(),
          ToPhysicalSize(Size(), GetWritingMode()), linebox.Size());
      GatherInlineContainerFragmentsFromLinebox(inline_containing_block_map,
                                                &containing_linebox_map,
                                                linebox, linebox_offset);
    } else if (child.fragment->IsBox()) {
      const auto& box_fragment = To<NGPhysicalBoxFragment>(*child.fragment);
      bool is_anonymous_container =
          box_fragment.GetLayoutObject() &&
          box_fragment.GetLayoutObject()->IsAnonymousBlock();
      if (!is_anonymous_container)
        continue;
      // If child is an anonymous container, this might be a special case of
      // split inlines. The inline container fragments might be inside
      // anonymous boxes. To find inline container fragments, traverse
      // lineboxes inside anonymous box.
      // For more on this special case, see "css container is an inline, with
      // inline splitting" comment in NGOutOfFlowLayoutPart::LayoutDescendant.
      const PhysicalOffset box_offset = child.offset.ConvertToPhysical(
          GetWritingMode(), Direction(),
          ToPhysicalSize(Size(), GetWritingMode()), box_fragment.Size());

      // Traverse lineboxes of anonymous box.
      for (const auto& box_child : box_fragment.Children()) {
        if (box_child->IsLineBox()) {
          const auto& linebox = To<NGPhysicalLineBoxFragment>(*box_child);
          const PhysicalOffset linebox_offset = box_child.Offset() + box_offset;
          GatherInlineContainerFragmentsFromLinebox(inline_containing_block_map,
                                                    &containing_linebox_map,
                                                    linebox, linebox_offset);
        }
      }
    }
  }
}

void NGBoxFragmentBuilder::ComputeInlineContainerGeometry(
    InlineContainingBlockMap* inline_containing_block_map) {
  if (inline_containing_block_map->IsEmpty())
    return;

  // This function requires that we have the final size of the fragment set
  // upon the builder.
  DCHECK_GE(InlineSize(), LayoutUnit());
  DCHECK_GE(BlockSize(), LayoutUnit());

#if DCHECK_IS_ON()
  // Make sure all entries are a continuation root.
  for (const auto& entry : *inline_containing_block_map)
    DCHECK_EQ(entry.key, entry.key->ContinuationRoot());
#endif

  HashMap<const LayoutObject*, LineBoxPair> containing_linebox_map;

  if (items_builder_) {
    // To access the items correctly we need to convert them to the physical
    // coordinate space.
    GatherInlineContainerFragmentsFromItems(
        items_builder_->Items(GetWritingMode(), Direction(),
                              ToPhysicalSize(Size(), GetWritingMode())),
        PhysicalOffset(), inline_containing_block_map, &containing_linebox_map);
    return;
  }

  // If we have children which are anonymous block, we might contain split
  // inlines, this can occur in the following example:
  // <div>
  //    Some text <span style="position: relative;">text
  //    <div>block</div>
  //    text </span> text.
  // </div>
  for (const auto& child : children_) {
    if (!child.fragment->IsAnonymousBlock())
      continue;

    const auto& child_fragment = To<NGPhysicalBoxFragment>(*child.fragment);
    const auto* items = child_fragment.Items();
    if (!items)
      continue;

    const PhysicalOffset child_offset = child.offset.ConvertToPhysical(
        GetWritingMode(), Direction(), ToPhysicalSize(Size(), GetWritingMode()),
        child_fragment.Size());
    GatherInlineContainerFragmentsFromItems(items->Items(), child_offset,
                                            inline_containing_block_map,
                                            &containing_linebox_map);
  }
}

void NGBoxFragmentBuilder::SetLastBaselineToBlockEndMarginEdgeIfNeeded() {
  if (ConstraintSpace()->BaselineAlgorithmType() !=
      NGBaselineAlgorithmType::kInlineBlock)
    return;

  if (!node_.UseBlockEndMarginEdgeForInlineBlockBaseline())
    return;

  // When overflow is present (within an atomic-inline baseline context) we
  // should always use the block-end margin edge as the baseline.
  NGBoxStrut margins = ComputeMarginsForSelf(*ConstraintSpace(), Style());
  SetLastBaseline(BlockSize() + margins.block_end);
}

#if DCHECK_IS_ON()

void NGBoxFragmentBuilder::CheckNoBlockFragmentation() const {
  DCHECK(!did_break_);
  DCHECK(!has_forced_break_);
  DCHECK_EQ(consumed_block_size_, LayoutUnit());
  DCHECK_EQ(minimal_space_shortage_, LayoutUnit::Max());
  DCHECK_EQ(initial_break_before_, EBreakBetween::kAuto);
  DCHECK_EQ(previous_break_after_, EBreakBetween::kAuto);
}

#endif

}  // namespace blink
