// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_container_fragment_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

namespace {

bool IsInlineContainerForNode(const NGBlockNode& node,
                              const LayoutObject* inline_container) {
  return inline_container && inline_container->IsLayoutInline() &&
         inline_container->CanContainOutOfFlowPositionedElement(
             node.Style().GetPosition());
}

}  // namespace

void NGContainerFragmentBuilder::ReplaceChild(
    wtf_size_t index,
    const NGPhysicalFragment& new_child,
    const LogicalOffset offset) {
  DCHECK_LT(index, children_.size());
  children_[index] = ChildWithOffset(offset, std::move(&new_child));
}

// Propagate data in |child| to this fragment. The |child| will then be added as
// a child fragment or a child fragment item.
void NGContainerFragmentBuilder::PropagateChildData(
    const NGPhysicalFragment& child,
    LogicalOffset child_offset,
    LogicalOffset relative_offset,
    const NGInlineContainer<LogicalOffset>* inline_container,
    absl::optional<LayoutUnit> adjustment_for_oof_propagation) {
  if (adjustment_for_oof_propagation &&
      NeedsOOFPositionedInfoPropagation(child)) {
    PropagateOOFPositionedInfo(child, child_offset, relative_offset,
                               /* offset_adjustment */ LogicalOffset(),
                               inline_container,
                               *adjustment_for_oof_propagation);
  }

  // We only need to report if inflow or floating elements depend on the
  // percentage resolution block-size. OOF-positioned children resolve their
  // percentages against the "final" size of their parent.
  if (!has_descendant_that_depends_on_percentage_block_size_) {
    if (child.DependsOnPercentageBlockSize() && !child.IsOutOfFlowPositioned())
      has_descendant_that_depends_on_percentage_block_size_ = true;

    // We may have a child which has the following style:
    // <div style="position: relative; top: 50%;"></div>
    // We need to mark this as depending on our %-block-size for the its offset
    // to be correctly calculated. This is *slightly* too broad as it only
    // depends on the available block-size, rather than the %-block-size.
    const auto& child_style = child.Style();
    if (child.IsCSSBox() && child_style.GetPosition() == EPosition::kRelative) {
      if (IsHorizontalWritingMode(Style().GetWritingMode())) {
        if (child_style.Top().IsPercentOrCalc() ||
            child_style.Bottom().IsPercentOrCalc())
          has_descendant_that_depends_on_percentage_block_size_ = true;
      } else {
        if (child_style.Left().IsPercentOrCalc() ||
            child_style.Right().IsPercentOrCalc())
          has_descendant_that_depends_on_percentage_block_size_ = true;
      }
    }
  }

  // Compute |has_floating_descendants_for_paint_| to optimize tree traversal
  // in paint.
  if (!has_floating_descendants_for_paint_) {
    if (child.IsFloating() || child.IsLegacyLayoutRoot() ||
        (child.HasFloatingDescendantsForPaint() &&
         !child.IsPaintedAtomically()))
      has_floating_descendants_for_paint_ = true;
  }

  // The |has_adjoining_object_descendants_| is used to determine if a fragment
  // can be re-used when preceding floats are present.
  // If a fragment doesn't have any adjoining object descendants, and is
  // self-collapsing, it can be "shifted" anywhere.
  if (!has_adjoining_object_descendants_) {
    if (!child.IsFormattingContextRoot() &&
        child.HasAdjoiningObjectDescendants())
      has_adjoining_object_descendants_ = true;
  }

  // Collect any (block) break tokens, but skip break tokens for fragmentainers,
  // as they should only escape a fragmentation context at the discretion of the
  // fragmentation context. Also skip this if there's a pre-set break token.
  if (has_block_fragmentation_ && !child.IsFragmentainerBox() &&
      !break_token_) {
    const NGBreakToken* child_break_token = child.BreakToken();
    switch (child.Type()) {
      case NGPhysicalFragment::kFragmentBox:
        if (child_break_token)
          child_break_tokens_.push_back(child_break_token);
        break;
      case NGPhysicalFragment::kFragmentLineBox:
        // We only care about the break token from the last line box added. This
        // is where we'll resume if we decide to block-fragment. Note that
        // child_break_token is nullptr if this is the last line to be generated
        // from the node.
        last_inline_break_token_ = To<NGInlineBreakToken>(child_break_token);
        line_count_++;
        break;
    }
  }
}

void NGContainerFragmentBuilder::AddChildInternal(
    scoped_refptr<const NGPhysicalFragment> child,
    const LogicalOffset& child_offset) {
  // In order to know where list-markers are within the children list (for the
  // |NGSimplifiedLayoutAlgorithm|) we always place them as the first child.
  if (child->IsListMarker()) {
    children_.push_front(ChildWithOffset(child_offset, std::move(child)));
    return;
  }

  if (child->IsTextControlPlaceholder()) {
    // ::placeholder should be followed by another block in order to paint
    // ::placeholder earlier.
    const wtf_size_t size = children_.size();
    if (size > 0) {
      children_.insert(size - 1,
                       ChildWithOffset(child_offset, std::move(child)));
      return;
    }
  }

  children_.emplace_back(child_offset, std::move(child));
}

void NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    NGLogicalStaticPosition::InlineEdge inline_edge,
    NGLogicalStaticPosition::BlockEdge block_edge,
    bool needs_block_offset_adjustment) {
  DCHECK(child);

  // If an OOF-positioned candidate has a static-position which uses a
  // non-block-start edge, we may need to adjust its static-position when the
  // final block-size is known.
  needs_block_offset_adjustment &=
      block_edge != NGLogicalStaticPosition::BlockEdge::kBlockStart;
  has_oof_candidate_that_needs_block_offset_adjustment_ |=
      needs_block_offset_adjustment;

  oof_positioned_candidates_.emplace_back(
      child, NGLogicalStaticPosition{child_offset, inline_edge, block_edge},
      NGInlineContainer<LogicalOffset>(), needs_block_offset_adjustment,
      /* containing_block */ NGContainingBlock<LogicalOffset>(),
      /* fixedpos_containing_block */ NGContainingBlock<LogicalOffset>());
}

void NGContainerFragmentBuilder::AddOutOfFlowChildCandidate(
    const NGLogicalOutOfFlowPositionedNode& candidate) {
  oof_positioned_candidates_.emplace_back(candidate);
}

void NGContainerFragmentBuilder::AddOutOfFlowInlineChildCandidate(
    NGBlockNode child,
    const LogicalOffset& child_offset,
    TextDirection inline_container_direction) {
  DCHECK(node_.IsInline() || layout_object_->IsLayoutInline());

  // As all inline-level fragments are built in the line-logical coordinate
  // system (Direction() is kLtr), we need to know the direction of the
  // parent element to correctly determine an OOF childs static position.
  AddOutOfFlowChildCandidate(child, child_offset,
                             IsLtr(inline_container_direction)
                                 ? NGLogicalStaticPosition::kInlineStart
                                 : NGLogicalStaticPosition::kInlineEnd,
                             NGLogicalStaticPosition::kBlockStart);
}

void NGContainerFragmentBuilder::AddOutOfFlowFragmentainerDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_fragmentainer_descendants_.push_back(descendant);
}

void NGContainerFragmentBuilder::AddOutOfFlowDescendant(
    const NGLogicalOutOfFlowPositionedNode& descendant) {
  oof_positioned_descendants_.push_back(descendant);
}

void NGContainerFragmentBuilder::SwapOutOfFlowPositionedCandidates(
    Vector<NGLogicalOutOfFlowPositionedNode>* candidates) {
  DCHECK(candidates->IsEmpty());
  std::swap(oof_positioned_candidates_, *candidates);

  if (!has_oof_candidate_that_needs_block_offset_adjustment_)
    return;

  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  // We might have an OOF-positioned candidate whose static-position depends on
  // the final block-size of this fragment.
  DCHECK_NE(BlockSize(), kIndefiniteSize);
  for (auto& candidate : *candidates) {
    if (!candidate.needs_block_offset_adjustment)
      continue;

    if (candidate.static_position.block_edge == BlockEdge::kBlockCenter)
      candidate.static_position.offset.block_offset += BlockSize() / 2;
    else if (candidate.static_position.block_edge == BlockEdge::kBlockEnd)
      candidate.static_position.offset.block_offset += BlockSize();
    candidate.needs_block_offset_adjustment = false;
  }

  has_oof_candidate_that_needs_block_offset_adjustment_ = false;
}

void NGContainerFragmentBuilder::AddMulticolWithPendingOOFs(
    const NGBlockNode& multicol,
    NGMulticolWithPendingOOFs<LogicalOffset> multicol_info) {
  DCHECK(To<LayoutBlockFlow>(multicol.GetLayoutBox())->MultiColumnFlowThread());
  auto it = multicols_with_pending_oofs_.find(multicol.GetLayoutBox());
  if (it != multicols_with_pending_oofs_.end())
    return;
  multicols_with_pending_oofs_.insert(multicol.GetLayoutBox(), multicol_info);
}

void NGContainerFragmentBuilder::SwapMulticolsWithPendingOOFs(
    MulticolCollection* multicols_with_pending_oofs) {
  DCHECK(multicols_with_pending_oofs->IsEmpty());
  std::swap(multicols_with_pending_oofs_, *multicols_with_pending_oofs);
}

void NGContainerFragmentBuilder::SwapOutOfFlowFragmentainerDescendants(
    Vector<NGLogicalOutOfFlowPositionedNode>* descendants) {
  DCHECK(descendants->IsEmpty());
  DCHECK(!has_oof_candidate_that_needs_block_offset_adjustment_);
  std::swap(oof_positioned_fragmentainer_descendants_, *descendants);
}

void NGContainerFragmentBuilder::TransferOutOfFlowCandidates(
    NGContainerFragmentBuilder* destination_builder,
    LogicalOffset additional_offset,
    const NGMulticolWithPendingOOFs<LogicalOffset>* multicol) {
  for (auto& candidate : oof_positioned_candidates_) {
    NGBlockNode node = candidate.Node();
    candidate.static_position.offset += additional_offset;
    if (multicol && multicol->fixedpos_containing_block.fragment &&
        node.Style().GetPosition() == EPosition::kFixed) {
      // A fixedpos containing block was found in |multicol|. Add the fixedpos
      // as a fragmentainer descendant instead.
      destination_builder->AddOutOfFlowFragmentainerDescendant(
          {node, candidate.static_position, candidate.inline_container,
           candidate.needs_block_offset_adjustment,
           multicol->fixedpos_containing_block,
           multicol->fixedpos_containing_block});
      continue;
    }
    destination_builder->AddOutOfFlowChildCandidate(candidate);
  }
  oof_positioned_candidates_.clear();
}

void NGContainerFragmentBuilder::MoveOutOfFlowDescendantCandidatesToDescendants(
    LogicalOffset relative_offset) {
  DCHECK(oof_positioned_descendants_.IsEmpty());
  std::swap(oof_positioned_candidates_, oof_positioned_descendants_);

  if (!layout_object_->IsInline())
    return;

  for (auto& candidate : oof_positioned_descendants_) {
    // If we are inside the inline algorithm, (and creating a fragment for a
    // <span> or similar), we may add a child (e.g. an atomic-inline) which has
    // OOF descandants.
    //
    // This checks if the object creating this box will be the container for
    // the given descendant.
    if (!candidate.inline_container.container &&
        IsInlineContainerForNode(candidate.Node(), layout_object_)) {
      candidate.inline_container = NGInlineContainer<LogicalOffset>(
          To<LayoutInline>(layout_object_), relative_offset);
    }

    // Ensure that the inline_container is a continuation root.
    if (candidate.inline_container.container) {
      candidate.inline_container.container = To<LayoutInline>(
          candidate.inline_container.container->ContinuationRoot());
    }
  }
}

void NGContainerFragmentBuilder::PropagateOOFPositionedInfo(
    const NGPhysicalFragment& fragment,
    LogicalOffset offset,
    LogicalOffset relative_offset,
    LogicalOffset offset_adjustment,
    const NGInlineContainer<LogicalOffset>* inline_container,
    LayoutUnit containing_block_adjustment,
    const NGContainingBlock<LogicalOffset>* fixedpos_containing_block,
    LogicalOffset additional_fixedpos_offset) {
  // Calling this method without any work to do is expensive, even if it ends up
  // skipping all its parts (probably due to its size). Make sure that we have a
  // reason to be here.
  DCHECK(NeedsOOFPositionedInfoPropagation(fragment));

  LogicalOffset adjusted_offset = offset + offset_adjustment + relative_offset;

  // Collect the child's out of flow descendants.
  const WritingModeConverter converter(GetWritingDirection(), fragment.Size());
  for (const auto& descendant : fragment.OutOfFlowPositionedDescendants()) {
    NGBlockNode node = descendant.Node();
    NGLogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(converter);

    NGInlineContainer<LogicalOffset> new_inline_container;
    if (descendant.inline_container.container) {
      new_inline_container.container = descendant.inline_container.container;
      new_inline_container.relative_offset =
          converter.ToLogical(descendant.inline_container.relative_offset,
                              PhysicalSize()) +
          relative_offset;
    } else if (inline_container &&
               IsInlineContainerForNode(node, inline_container->container)) {
      new_inline_container = *inline_container;
    }

    // If an OOF element is inside a fragmentation context, it will be laid out
    // once it reaches the fragmentation context root. However, if such OOF
    // elements have fixedpos descendants, those descendants will not find their
    // containing block if the containing block lives inside the fragmentation
    // context root. In this case, the containing block will be passed in via
    // |fixedpos_containing_block|. If one exists, add the fixedpos as a
    // fragmentainer descendant with the correct containing block and static
    // position. In the case of nested fragmentation, the fixedpos containing
    // block may be in an outer fragmentation context root. In such cases,
    // the fixedpos will be added as a fragmentainer descendant at a later time.
    // However, an |additional_fixedpos_offset| should be applied if one is
    // provided.
    if ((fixedpos_containing_block ||
         additional_fixedpos_offset != LogicalOffset()) &&
        node.Style().GetPosition() == EPosition::kFixed) {
      static_position.offset += additional_fixedpos_offset;
      // Relative offsets should be applied after fragmentation. However, if
      // there is any relative offset that occurrend before the fixedpos reached
      // its containing block, that relative offset should be applied to the
      // static position (before fragmentation).
      static_position.offset +=
          relative_offset - fixedpos_containing_block->relative_offset;
      if (fixedpos_containing_block && fixedpos_containing_block->fragment) {
        AddOutOfFlowFragmentainerDescendant(
            {node, static_position, new_inline_container,
             /* needs_block_offset_adjustment */ false,
             *fixedpos_containing_block, *fixedpos_containing_block});
        continue;
      }
    }
    static_position.offset += adjusted_offset;

    // |oof_positioned_candidates_| should not have duplicated entries.
    DCHECK(std::none_of(
        oof_positioned_candidates_.begin(), oof_positioned_candidates_.end(),
        [&node](const NGLogicalOutOfFlowPositionedNode& oof_node) {
          return oof_node.Node() == node;
        }));
    oof_positioned_candidates_.emplace_back(node, static_position,
                                            new_inline_container);
  }

  const NGPhysicalBoxFragment* box_fragment =
      DynamicTo<NGPhysicalBoxFragment>(&fragment);
  if (!box_fragment)
    return;

  if (box_fragment->HasMulticolsWithPendingOOFs()) {
    const auto& multicols_with_pending_oofs =
        box_fragment->MulticolsWithPendingOOFs();
    for (auto& multicol : multicols_with_pending_oofs) {
      auto& multicol_info = multicol.value;
      LogicalOffset multicol_offset =
          converter.ToLogical(multicol_info.multicol_offset, PhysicalSize());

      const NGPhysicalFragment* fixedpos_containing_block_fragment =
          multicol_info.fixedpos_containing_block.fragment.get();
      if (!fixedpos_containing_block_fragment &&
          box_fragment->GetLayoutObject() &&
          box_fragment->GetLayoutObject()->CanContainFixedPositionObjects())
        fixedpos_containing_block_fragment = box_fragment;

      // If a fixedpos containing block was found, the |multicol_offset|
      // should remain relative to the fixedpos containing block. Otherwise,
      // continue to adjust the |multicol_offset| to be relative to the current
      // |fragment|.
      LogicalOffset fixedpos_containing_block_offset;
      LogicalOffset fixedpos_containing_block_rel_offset;
      if (fixedpos_containing_block_fragment) {
        fixedpos_containing_block_offset =
            converter.ToLogical(multicol_info.fixedpos_containing_block.offset,
                                fixedpos_containing_block_fragment->Size());
        fixedpos_containing_block_rel_offset = converter.ToLogical(
            multicol_info.fixedpos_containing_block.relative_offset,
            fixedpos_containing_block_fragment->Size());
        fixedpos_containing_block_rel_offset += relative_offset;
        // We want the fixedpos containing block offset to be the offset from
        // the containing block to the top of the fragmentation context root,
        // such that it includes the block offset contributions of previous
        // fragmentainers. The block contribution from previous fragmentainers
        // has already been applied. As such, avoid unnecessarily adding an
        // additional inline/block offset of any fragmentainers.
        if (!box_fragment->IsFragmentainerBox())
          fixedpos_containing_block_offset += offset;
        fixedpos_containing_block_offset.block_offset +=
            containing_block_adjustment;
      } else {
        multicol_offset += adjusted_offset;
      }
      AddMulticolWithPendingOOFs(
          NGBlockNode(multicol.key),
          NGMulticolWithPendingOOFs<LogicalOffset>(
              multicol_offset, NGContainingBlock<LogicalOffset>(
                                   fixedpos_containing_block_offset,
                                   fixedpos_containing_block_rel_offset,
                                   fixedpos_containing_block_fragment)));
    }
  }

  if (!box_fragment->HasOutOfFlowPositionedFragmentainerDescendants())
    return;

  const auto& out_of_flow_fragmentainer_descendants =
      box_fragment->OutOfFlowPositionedFragmentainerDescendants();
  for (const auto& descendant : out_of_flow_fragmentainer_descendants) {
    const NGPhysicalFragment* containing_block_fragment =
        descendant.containing_block.fragment.get();
    if (!containing_block_fragment) {
      containing_block_fragment = box_fragment;
    } else if (box_fragment->IsFragmentationContextRoot()) {
      // If we find a multicol with OOF positioned fragmentainer descendants,
      // then that multicol is an inner multicol with pending OOFs. Those OOFs
      // will be laid out inside the inner multicol when we reach the outermost
      // fragmentation context, so we should not propagate those OOFs up the
      // tree any further.
      continue;
    }

    LogicalOffset containing_block_offset = converter.ToLogical(
        descendant.containing_block.offset, containing_block_fragment->Size());
    LogicalOffset containing_block_rel_offset =
        converter.ToLogical(descendant.containing_block.relative_offset,
                            containing_block_fragment->Size());
    containing_block_rel_offset += relative_offset;
    if (!box_fragment->IsFragmentainerBox())
      containing_block_offset += offset;
    containing_block_offset.block_offset += containing_block_adjustment;

    const NGPhysicalFragment* fixedpos_containing_block_fragment =
        descendant.fixedpos_containing_block.fragment.get();
    if (!fixedpos_containing_block_fragment &&
        box_fragment->GetLayoutObject() &&
        box_fragment->GetLayoutObject()->CanContainFixedPositionObjects())
      fixedpos_containing_block_fragment = box_fragment;

    LogicalOffset fixedpos_containing_block_offset;
    LogicalOffset fixedpos_containing_block_rel_offset;
    if (fixedpos_containing_block_fragment) {
      fixedpos_containing_block_offset =
          converter.ToLogical(descendant.fixedpos_containing_block.offset,
                              fixedpos_containing_block_fragment->Size());
      fixedpos_containing_block_rel_offset = converter.ToLogical(
          descendant.fixedpos_containing_block.relative_offset,
          fixedpos_containing_block_fragment->Size());
      fixedpos_containing_block_rel_offset += relative_offset;
      if (!box_fragment->IsFragmentainerBox())
        fixedpos_containing_block_offset += offset;
      fixedpos_containing_block_offset.block_offset +=
          containing_block_adjustment;
    }

    if (!fixedpos_containing_block_fragment && fixedpos_containing_block) {
      fixedpos_containing_block_fragment =
          fixedpos_containing_block->fragment.get();
      fixedpos_containing_block_offset = fixedpos_containing_block->offset;
      fixedpos_containing_block_rel_offset =
          fixedpos_containing_block->relative_offset;
    }

    LogicalOffset inline_relative_offset = converter.ToLogical(
        descendant.inline_container.relative_offset, PhysicalSize());
    NGInlineContainer<LogicalOffset> new_inline_container(
        descendant.inline_container.container, inline_relative_offset);

    // The static position should remain relative to its containing block
    // fragment.
    const WritingModeConverter containing_block_converter(
        GetWritingDirection(), containing_block_fragment->Size());
    NGLogicalStaticPosition static_position =
        descendant.StaticPosition().ConvertToLogical(
            containing_block_converter);

    // The relative offset should be applied after fragmentation. Subtract out
    // the accumulated relative offset from the inline container to the
    // containing block so that it can be re-applied at the correct time.
    if (new_inline_container.container &&
        containing_block_fragment == box_fragment)
      static_position.offset -= inline_relative_offset;

    AddOutOfFlowFragmentainerDescendant(
        {descendant.Node(), static_position, new_inline_container,
         /* needs_block_offset_adjustment */ false,
         NGContainingBlock<LogicalOffset>(containing_block_offset,
                                          containing_block_rel_offset,
                                          containing_block_fragment),
         NGContainingBlock<LogicalOffset>(fixedpos_containing_block_offset,
                                          fixedpos_containing_block_rel_offset,
                                          fixedpos_containing_block_fragment)});
  }
}

scoped_refptr<const NGLayoutResult> NGContainerFragmentBuilder::Abort(
    NGLayoutResult::EStatus status) {
  return base::AdoptRef(new NGLayoutResult(
      NGLayoutResult::NGContainerFragmentBuilderPassKey(), status, this));
}

#if DCHECK_IS_ON()

String NGContainerFragmentBuilder::ToString() const {
  StringBuilder builder;
  builder.AppendFormat("ContainerFragment %.2fx%.2f, Children %u\n",
                       InlineSize().ToFloat(), BlockSize().ToFloat(),
                       children_.size());
  for (auto& child : children_) {
    builder.Append(child.fragment->DumpFragmentTree(
        NGPhysicalFragment::DumpAll & ~NGPhysicalFragment::DumpHeaderText));
  }
  return builder.ToString();
}

#endif

}  // namespace blink
