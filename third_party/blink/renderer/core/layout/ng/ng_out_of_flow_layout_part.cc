// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_absolute_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_descendant.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    NGBoxFragmentBuilder* container_builder,
    bool contains_absolute,
    bool contains_fixed,
    const NGBoxStrut& border_scrollbar,
    const NGConstraintSpace& container_space,
    const ComputedStyle& container_style,
    base::Optional<NGLogicalSize> initial_containing_block_fixed_size)
    : container_builder_(container_builder),
      contains_absolute_(contains_absolute),
      contains_fixed_(contains_fixed) {
  if (!container_builder->HasOutOfFlowDescendantCandidates())
    return;

  NGPhysicalBoxStrut physical_border_scrollbar =
      border_scrollbar.ConvertToPhysical(container_style.GetWritingMode(),
                                         container_style.Direction());

  default_containing_block_.style = &container_style;
  default_containing_block_.content_size_for_absolute =
      container_builder_->Size();
  default_containing_block_.content_size_for_absolute.inline_size =
      std::max(default_containing_block_.content_size_for_absolute.inline_size -
                   border_scrollbar.InlineSum(),
               LayoutUnit());
  default_containing_block_.content_size_for_absolute.block_size =
      std::max(default_containing_block_.content_size_for_absolute.block_size -
                   border_scrollbar.BlockSum(),
               LayoutUnit());
  default_containing_block_.content_size_for_fixed =
      initial_containing_block_fixed_size
          ? initial_containing_block_fixed_size.value()
          : default_containing_block_.content_size_for_absolute;

  default_containing_block_.container_offset = NGLogicalOffset(
      border_scrollbar.inline_start, border_scrollbar.block_start);
  default_containing_block_.physical_container_offset = NGPhysicalOffset(
      physical_border_scrollbar.left, physical_border_scrollbar.top);
}

void NGOutOfFlowLayoutPart::Run(LayoutBox* only_layout) {
  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  const LayoutObject* current_container = container_builder_->GetLayoutObject();

  container_builder_->GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates, current_container);

  // Special case: containing block is a split inline.
  // Detailed explanation of what this means is inside
  // NGOutOfFlowLayoutPart::LayoutDescendant.
  // This code can be removed once we stop inline splitting.
  // only_layout is only true when positioning blocks for Legacy.
  // Cross-anonymous inline containers cannot be done in Legacy.
  if (descendant_candidates.size() > 0 && current_container && !only_layout &&
      current_container->IsAnonymousBlock() &&
      current_container->IsRelPositioned()) {
    // Comments and code copied from
    // LayoutBox::ContainingBlockLogicalWidthForPositioned.
    // Ensure we compute our width based on the width of our rel-pos inline
    // container rather than any anonymous block created to manage a block-flow
    // ancestor of ours in the rel-pos inline's inline flow.
    LayoutBoxModelObject* absolute_containing_block =
        ToLayoutBox(current_container)->Continuation();
    // There may be nested parallel inline continuations. We have now found the
    // innermost inline (which may not be relatively positioned). Locate the
    // inline that serves as the containing block of this box.
    while (!absolute_containing_block->CanContainOutOfFlowPositionedElement(
        EPosition::kAbsolute)) {
      absolute_containing_block =
          ToLayoutBoxModelObject(absolute_containing_block->Container());
    }
    DCHECK(absolute_containing_block->IsLayoutInline());
    // Make absolute_containing_block continuation root.
    absolute_containing_block = ToLayoutBoxModelObject(
        absolute_containing_block->GetNode()->GetLayoutObject());
    for (auto& candidate : descendant_candidates) {
      if (absolute_containing_block->CanContainOutOfFlowPositionedElement(
              candidate.node.Style().GetPosition())) {
        candidate.inline_container = absolute_containing_block;
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }
    return;
  }

  while (descendant_candidates.size() > 0) {
    ComputeInlineContainingBlocks(descendant_candidates);
    for (auto& candidate : descendant_candidates) {
      if (IsContainingBlockForDescendant(candidate) &&
          (!only_layout || candidate.node.GetLayoutBox() == only_layout)) {
        NGLogicalOffset offset;
        scoped_refptr<NGLayoutResult> result =
            LayoutDescendant(candidate, &offset, only_layout);
        container_builder_->AddChild(*result, offset);
        if (candidate.node.GetLayoutBox() != only_layout)
          candidate.node.UseOldOutOfFlowPositioning();
      } else {
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }
    // Sweep any descendants that might have been added.
    // This happens when an absolute container has a fixed child.
    descendant_candidates.Shrink(0);
    container_builder_->GetAndClearOutOfFlowDescendantCandidates(
        &descendant_candidates, current_container);
  }
}

NGOutOfFlowLayoutPart::ContainingBlockInfo
NGOutOfFlowLayoutPart::GetContainingBlockInfo(
    const NGOutOfFlowPositionedDescendant& descendant) const {
  // TODO remove containing_blocks_map_.Contains
  if (descendant.inline_container &&
      containing_blocks_map_.Contains(descendant.inline_container)) {
    DCHECK(containing_blocks_map_.Contains(descendant.inline_container));
    return containing_blocks_map_.at(descendant.inline_container);
  }
  return default_containing_block_;
}

void NGOutOfFlowLayoutPart::ComputeInlineContainingBlocks(
    Vector<NGOutOfFlowPositionedDescendant> descendants) {
  NGBoxFragmentBuilder::InlineContainingBlockMap inline_container_fragments;

  for (auto& descendant : descendants) {
    if (descendant.inline_container &&
        !inline_container_fragments.Contains(descendant.inline_container)) {
      NGBoxFragmentBuilder::InlineContainingBlockGeometry inline_geometry = {};
      inline_container_fragments.insert(descendant.inline_container,
                                        inline_geometry);
    }
  }
  // Fetch start/end fragment info.
  container_builder_->ComputeInlineContainerFragments(
      &inline_container_fragments);
  NGLogicalSize container_builder_size = container_builder_->Size();
  NGPhysicalSize container_builder_physical_size =
      ToNGPhysicalSize(container_builder_size,
                       default_containing_block_.style->GetWritingMode());
  // Translate start/end fragments into ContainingBlockInfo.
  for (auto& block_info : inline_container_fragments) {
    // Variables needed to describe ContainingBlockInfo
    const ComputedStyle* inline_cb_style = block_info.key->Style();
    NGLogicalSize inline_cb_size;
    NGLogicalOffset container_offset;
    NGPhysicalOffset physical_container_offset;

    if (!block_info.value.has_value()) {
      // This happens when Legacy block is the default container.
      // In this case, container builder does not have any fragments because
      // ng layout algorithm did not run.
      DCHECK(block_info.key->IsLayoutInline());
      NOTIMPLEMENTED()
          << "Inline containing block might need geometry information";
      // TODO(atotic) ContainingBlockInfo geometry
      // must be computed from Legacy algorithm
    } else {
      DCHECK(inline_cb_style);

      // TODO Creating dummy constraint space just to get borders feels wrong.
      NGConstraintSpace dummy_constraint_space =
          NGConstraintSpaceBuilder(inline_cb_style->GetWritingMode(),
                                   inline_cb_style->GetWritingMode(),
                                   /* is_new_fc */ false)
              .ToConstraintSpace();
      NGBoxStrut inline_cb_borders =
          ComputeBorders(dummy_constraint_space, *inline_cb_style);

      // The calculation below determines the size of the inline containing
      // block rect.
      //
      // To perform this calculation we:
      // 1. Determine the start_offset "^", this is at the logical-start (wrt.
      //    default containing block), of the start fragment rect.
      // 2. Determine the end_offset "$", this is at the logical-end (wrt.
      //    default containing block), of the end  fragment rect.
      // 3. Determine the logical rectangle defined by these two offsets.
      //
      // Case 1a: Same direction, overlapping fragments.
      //      +---------------
      // ---> |^*****-------->
      //      +*----*---------
      //       *    *
      // ------*----*+
      // ----> *****$| --->
      // ------------+
      //
      // Case 1b: Different direction, overlapping fragments.
      //      +---------------
      // ---> ^******* <-----|
      //      *------*--------
      //      *      *
      // -----*------*
      // |<-- *******$ --->
      // ------------+
      //
      // Case 2a: Same direction, non-overlapping fragments.
      //             +--------
      // --------->  |^ ----->
      //             +*-------
      //              *
      // --------+    *
      // ------->|    $ --->
      // --------+
      //
      // Case 2b: Same direction, non-overlapping fragments.
      //             +--------
      // --------->  ^ <-----|
      //             *--------
      //             *
      // --------+   *
      // | <------   $  --->
      // --------+
      //
      // Note in cases [1a, 2a] we need to account for the inline borders of
      // the rectangles, where-as in [1b, 2b] we do not. This is handled by the
      // is_same_direction check(s).
      //
      // Note in cases [2a, 2b] we don't allow a "negative" containing block
      // size, we clamp negative sizes to zero.
      WritingMode container_writing_mode =
          default_containing_block_.style->GetWritingMode();
      TextDirection container_direction =
          default_containing_block_.style->Direction();

      bool is_same_direction =
          container_direction == inline_cb_style->Direction();

      // Step 1 - determine the start_offset.
      const NGPhysicalOffsetRect& start_rect =
          block_info.value.value().start_fragment_union_rect;
      NGLogicalOffset start_offset = start_rect.offset.ConvertToLogical(
          container_writing_mode, container_direction,
          container_builder_physical_size, start_rect.size);

      // Make sure we add the inline borders, we don't need to do this in the
      // inline direction if the blocks are in opposite directions.
      start_offset.block_offset += inline_cb_borders.block_start;
      if (is_same_direction)
        start_offset.inline_offset += inline_cb_borders.inline_start;

      // Step 2 - determine the end_offset.
      const NGPhysicalOffsetRect& end_rect =
          block_info.value.value().end_fragment_union_rect;
      NGLogicalOffset end_offset = end_rect.offset.ConvertToLogical(
          container_writing_mode, container_direction,
          container_builder_physical_size, end_rect.size);

      // Add in the size of the fragment to get the logical end of the fragment.
      end_offset += end_rect.size.ConvertToLogical(container_writing_mode);

      // Make sure we substract the inline borders, we don't need to do this in
      // the inline direction if the blocks are in opposite directions.
      end_offset.block_offset -= inline_cb_borders.block_end;
      if (is_same_direction)
        end_offset.inline_offset -= inline_cb_borders.inline_end;

      // Make sure we don't end up with a rectangle with "negative" size.
      end_offset.inline_offset =
          std::max(end_offset.inline_offset, start_offset.inline_offset);

      // Step 3 - determine the logical rectange.

      // Determine the logical size of the containing block.
      inline_cb_size = {end_offset.inline_offset - start_offset.inline_offset,
                        end_offset.block_offset - start_offset.block_offset};
      DCHECK_GE(inline_cb_size.inline_size, LayoutUnit());
      DCHECK_GE(inline_cb_size.block_size, LayoutUnit());

      // Determine the container offsets.
      container_offset = start_offset;
      physical_container_offset = container_offset.ConvertToPhysical(
          container_writing_mode, container_direction,
          container_builder_physical_size,
          ToNGPhysicalSize(inline_cb_size, container_writing_mode));
    }
    containing_blocks_map_.insert(
        block_info.key,
        ContainingBlockInfo{inline_cb_style, inline_cb_size, inline_cb_size,
                            container_offset, physical_container_offset});
  }
}

scoped_refptr<NGLayoutResult> NGOutOfFlowLayoutPart::LayoutDescendant(
    const NGOutOfFlowPositionedDescendant& descendant,
    NGLogicalOffset* offset,
    LayoutBox* only_layout) {
  ContainingBlockInfo container_info = GetContainingBlockInfo(descendant);

  WritingMode container_writing_mode(container_info.style->GetWritingMode());
  WritingMode descendant_writing_mode(descendant.node.Style().GetWritingMode());

  // Adjust the static_position (which is currently relative to the default
  // container's border-box). ng_absolute_utils expects the static position to
  // be relative to the container's padding-box.
  NGStaticPosition static_position(descendant.static_position);
  static_position.offset -= container_info.physical_container_offset;

  NGLogicalSize container_content_size =
      container_info.ContentSize(descendant.node.Style().GetPosition());

  NGConstraintSpace descendant_constraint_space =
      NGConstraintSpaceBuilder(container_writing_mode, descendant_writing_mode,
                               /* is_new_fc */ true)
          .SetTextDirection(container_info.style->Direction())
          .SetAvailableSize(container_content_size)
          .SetPercentageResolutionSize(container_content_size)
          .ToConstraintSpace();

  // The block_estimate is in the descendant's writing mode.
  base::Optional<LayoutUnit> block_estimate;
  base::Optional<MinMaxSize> min_max_size;

  scoped_refptr<NGLayoutResult> layout_result = nullptr;

  NGBlockNode node = descendant.node;
  if (AbsoluteNeedsChildInlineSize(descendant.node.Style()) ||
      NeedMinMaxSize(descendant.node.Style()) ||
      descendant.node.ShouldBeConsideredAsReplaced()) {
    // This is a new formatting context, so whatever happened on the outside
    // doesn't concern us.
    MinMaxSizeInput zero_input;
    min_max_size = node.ComputeMinMaxSize(descendant_writing_mode, zero_input,
                                          &descendant_constraint_space);
  }

  base::Optional<NGLogicalSize> replaced_size;
  if (descendant.node.IsReplaced()) {
    replaced_size = ComputeReplacedSize(
        descendant.node, descendant_constraint_space, min_max_size);
  } else if (descendant.node.ShouldBeConsideredAsReplaced()) {
    replaced_size = NGLogicalSize{
        min_max_size->ShrinkToFit(
            descendant_constraint_space.AvailableSize().inline_size),
        NGSizeIndefinite};
  }
  NGAbsolutePhysicalPosition node_position =
      ComputePartialAbsoluteWithChildInlineSize(
          descendant_constraint_space, descendant.node.Style(), static_position,
          min_max_size, replaced_size, container_writing_mode,
          container_info.style->Direction());

  // ShouldBeConsideredAsReplaced sets inline size.
  // It does not set block size. This is a compatiblity quirk.
  if (!descendant.node.IsReplaced() &&
      descendant.node.ShouldBeConsideredAsReplaced())
    replaced_size.reset();

  if (AbsoluteNeedsChildBlockSize(descendant.node.Style())) {
    layout_result = GenerateFragment(descendant.node, container_info,
                                     block_estimate, node_position);

    DCHECK(layout_result->PhysicalFragment());
    NGFragment fragment(descendant_writing_mode,
                        *layout_result->PhysicalFragment());

    block_estimate = fragment.BlockSize();
  }

  ComputeFullAbsoluteWithChildBlockSize(
      descendant_constraint_space, descendant.node.Style(), static_position,
      block_estimate, replaced_size, container_writing_mode,
      container_info.style->Direction(), &node_position);

  // Skip this step if we produced a fragment when estimating the block size.
  if (!layout_result) {
    block_estimate =
        node_position.size.ConvertToLogical(descendant_writing_mode).block_size;
    layout_result = GenerateFragment(descendant.node, container_info,
                                     block_estimate, node_position);
  }
  if (node.GetLayoutBox()->IsLayoutNGObject()) {
    ToLayoutBlock(node.GetLayoutBox())
        ->SetIsLegacyInitiatedOutOfFlowLayout(false);
  }

  NGBoxStrut inset = node_position.inset.ConvertToLogical(
      container_writing_mode, default_containing_block_.style->Direction());

  // inset is relative to the container's padding-box. Convert this to being
  // relative to the default container's border-box.
  offset->inline_offset =
      inset.inline_start + container_info.container_offset.inline_offset;
  offset->block_offset =
      inset.block_start + container_info.container_offset.block_offset;

  base::Optional<LayoutUnit> y = ComputeAbsoluteDialogYPosition(
      *descendant.node.GetLayoutBox(),
      layout_result->PhysicalFragment()->Size().height);
  if (y.has_value()) {
    if (IsHorizontalWritingMode(container_writing_mode))
      offset->block_offset = y.value();
    else
      offset->inline_offset = y.value();
  }

  // Special case: oof css container is a split inline.
  // When css container spans multiple anonymous blocks, its dimensions
  // can only be computed by a block that is an ancestor of all fragments
  // generated by css container. That block is parent of anonymous containing
  // block.
  // That is why instead of OOF being placed by its anononymous container,
  // they get placed by anonymous container's parent.
  // This is different from all other OOF blocks, and requires special
  // handling in several places in the OOF code.
  // There is an exception to special case: if anonymous block is Legacy,
  // we cannot do the fancy multiple anonymous block traversal, and we handle
  // it like regular blocks.
  //
  // Detailed example:
  //
  // If Layout tree looks like this:
  // LayoutNGBlockFlow#container
  //   LayoutNGBlockFlow (anonymous#1)
  //     LayoutInline#1 (relative)
  //   LayoutNGBlockFlow (anonymous#2 relative)
  //     LayoutNGBlockFlow#oof (positioned)
  //   LayoutNGBlockFlow (anonymous#3)
  //     LayoutInline#3 (continuation)
  //
  // The containing block geometry is defined by split inlines,
  // LayoutInline#1, LayoutInline#3.
  // Css container anonymous#2 does not have information needed
  // to compute containing block geometry.
  // Therefore, #oof cannot be placed by anonymous#2. NG handles this case
  // by placing #oof in parent of anonymous (#container).
  //
  // But, PaintPropertyTreeBuilder expects #oof.Location() to be wrt
  // css container, #anonymous2. This is why the code below adjusts
  // the legacy offset from being wrt #container to being wrt #anonymous2.

  const LayoutObject* container = descendant.node.GetLayoutBox()->Container();
  if (!only_layout && container->IsAnonymousBlock()) {
    NGLogicalOffset container_offset =
        container_builder_->GetChildOffset(container);
    *offset -= container_offset;
  }
  return layout_result;
}

bool NGOutOfFlowLayoutPart::IsContainingBlockForDescendant(
    const NGOutOfFlowPositionedDescendant& descendant) {
  EPosition position = descendant.node.Style().GetPosition();

  // Descendants whose containing block is inline are always positioned
  // inside closest parent block flow.
  if (descendant.inline_container) {
    DCHECK(
        descendant.node.Style().GetPosition() == EPosition::kAbsolute &&
            descendant.inline_container->CanContainAbsolutePositionObjects() ||
        (descendant.node.Style().GetPosition() == EPosition::kFixed &&
         descendant.inline_container->CanContainFixedPositionObjects()));
    return true;
  }
  return (contains_absolute_ && position == EPosition::kAbsolute) ||
         (contains_fixed_ && position == EPosition::kFixed);
}

// The fragment is generated in one of these two scenarios:
// 1. To estimate descendant's block size, in this case block_size is
//    container's available size.
// 2. To compute final fragment, when block size is known from the absolute
//    position calculation.
scoped_refptr<NGLayoutResult> NGOutOfFlowLayoutPart::GenerateFragment(
    NGBlockNode descendant,
    const ContainingBlockInfo& container_info,
    const base::Optional<LayoutUnit>& block_estimate,
    const NGAbsolutePhysicalPosition& node_position) {
  // As the block_estimate is always in the descendant's writing mode, we build
  // the constraint space in the descendant's writing mode.
  WritingMode writing_mode(descendant.Style().GetWritingMode());
  NGLogicalSize container_size(
      ToNGPhysicalSize(
          container_info.ContentSize(descendant.Style().GetPosition()),
          default_containing_block_.style->GetWritingMode())
          .ConvertToLogical(writing_mode));

  LayoutUnit inline_size =
      node_position.size.ConvertToLogical(writing_mode).inline_size;
  LayoutUnit block_size =
      block_estimate ? *block_estimate : container_size.block_size;

  NGLogicalSize available_size{inline_size, block_size};

  // TODO(atotic) will need to be adjusted for scrollbars.
  NGConstraintSpaceBuilder builder(writing_mode, writing_mode,
                                   /* is_new_fc */ true);
  builder.SetAvailableSize(available_size)
      .SetTextDirection(descendant.Style().Direction())
      .SetPercentageResolutionSize(container_size)
      .SetIsFixedSizeInline(true);
  if (block_estimate)
    builder.SetIsFixedSizeBlock(true);
  NGConstraintSpace space = builder.ToConstraintSpace();

  scoped_refptr<NGLayoutResult> result = descendant.Layout(space);

  // Legacy Grid and Flexbox seem to handle oof margins correctly
  // on their own, and break if we set them here.
  if (!descendant.GetLayoutBox()
           ->ContainingBlock()
           ->Style()
           ->IsDisplayFlexibleOrGridBox())
    descendant.GetLayoutBox()->SetMargin(node_position.margins);

  return result;
}

}  // namespace blink
