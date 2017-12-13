// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_out_of_flow_layout_part.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_absolute_utils.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGOutOfFlowLayoutPart::NGOutOfFlowLayoutPart(
    const NGBlockNode container,
    const NGConstraintSpace& container_space,
    const ComputedStyle& container_style,
    NGFragmentBuilder* container_builder)
    : container_style_(container_style),
      container_builder_(container_builder),
      contains_absolute_(container.IsAbsoluteContainer()),
      contains_fixed_(container.IsFixedContainer()) {
  WritingMode writing_mode(container_style_.GetWritingMode());

  NGBoxStrut borders = ComputeBorders(container_space, container_style_);
  NGBoxStrut scrollers = container.GetScrollbarSizes();
  NGBoxStrut borders_and_scrollers = borders + scrollers;
  content_offset_ = NGLogicalOffset{borders_and_scrollers.inline_start,
                                    borders_and_scrollers.block_start};

  NGPhysicalBoxStrut physical_borders = borders_and_scrollers.ConvertToPhysical(
      writing_mode, container_style_.Direction());
  content_physical_offset_ =
      NGPhysicalOffset(physical_borders.left, physical_borders.top);

  container_size_ = container_builder_->Size();
  container_size_.inline_size -= borders_and_scrollers.InlineSum();
  container_size_.block_size -= borders_and_scrollers.BlockSum();

  icb_size_ = container_space.InitialContainingBlockSize();
}

void NGOutOfFlowLayoutPart::Run(bool update_legacy) {
  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  container_builder_->GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates);

  while (descendant_candidates.size() > 0) {
    ComputeInlineContainingBlocks(descendant_candidates);
    for (auto& candidate : descendant_candidates) {
      if (IsContainingBlockForDescendant(candidate)) {
        NGLogicalOffset offset;
        scoped_refptr<NGLayoutResult> result =
            LayoutDescendant(candidate, &offset);
        container_builder_->AddChild(std::move(result), offset);
        if (update_legacy)
          candidate.node.UseOldOutOfFlowPositioning();
      } else {
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }
    // Sweep any descendants that might have been added.
    // This happens when an absolute container has a fixed child.
    descendant_candidates.clear();
    container_builder_->GetAndClearOutOfFlowDescendantCandidates(
        &descendant_candidates);
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
  return ContainingBlockInfo{&container_style_, container_size_,
                             content_offset_, content_physical_offset_};
}

void NGOutOfFlowLayoutPart::ComputeInlineContainingBlocks(
    Vector<NGOutOfFlowPositionedDescendant> descendants) {
  HashMap<const LayoutObject*, NGFragmentBuilder::FragmentPair>
      inline_container_fragments;

  for (auto& descendant : descendants) {
    if (descendant.inline_container &&
        !inline_container_fragments.Contains(descendant.inline_container)) {
      NGFragmentBuilder::FragmentPair fragment_pair = {};
      inline_container_fragments.insert(descendant.inline_container,
                                        fragment_pair);
    }
  }
  // Fetch start/end fragment info.
  NGLogicalSize container_builder_size;
  container_builder_->ComputeInlineContainerFragments(
      &inline_container_fragments, &container_builder_size);
  NGPhysicalSize container_builder_physical_size =
      container_builder_size.ConvertToPhysical(
          container_style_.GetWritingMode());
  // Translate start/end fragments into ContainingBlockInfo.
  for (auto& block_info : inline_container_fragments) {
    NGLogicalOffset inline_content_offset;
    NGPhysicalOffset inline_content_physical_offset;
    NGLogicalSize inline_cb_size;
    const ComputedStyle* inline_cb_style;
    inline_cb_style = &block_info.value.start_fragment->Style();
    scoped_refptr<NGConstraintSpace> dummy_constraint_space =
        NGConstraintSpaceBuilder(inline_cb_style->GetWritingMode(), icb_size_)
            .ToConstraintSpace(inline_cb_style->GetWritingMode());
    // TODO Creating dummy constraint space just to get borders feels wrong.
    NGBoxStrut inline_cb_borders =
        ComputeBorders(*dummy_constraint_space, *inline_cb_style);
    NGPhysicalBoxStrut physical_borders = inline_cb_borders.ConvertToPhysical(
        inline_cb_style->GetWritingMode(), inline_cb_style->Direction());
    NGBoxStrut inline_cb_padding =
        ComputePadding(*dummy_constraint_space, *inline_cb_style);

    // Warning: lots of non-obvious coordinate manipulation ahead.
    //
    // High level goal is:
    // - Find logical topleft of start fragment, and logical bottomright
    //   of end fragment.
    // - Use these to compute inline-cb geometry:
    //   - inline-cb size (content_size)
    //   - inline-cb offset from containing block (default_container_offset)
    //
    // We start with:
    // start_fragment, which has physical offset from start_linebox_fragment,
    // end_fragment, also with physical offset.
    // start_linebox_fragment, which has logical offset from containing box.
    // end_linebox_fragment, which also has logical offset from containing box.
    //
    // Then magic happens.^H^H^H^H^H^H^H
    //
    // Then we do the following:
    // 1. Find start fragment physical topleft wrt containing box.
    //    - convert start_fragment offset to logical.
    //    - convert start fragment inline/block start to physical.
    //    - convert linebox topleft to physical.
    //    - add start fragment to linebox topleft
    // 2. Find end fragment bottom right wrt containing box
    //    - convert end fragment offset to logical.
    //    - convert end fragment inline/block end to physical
    //    - convert linebox topleft to physical
    //    - add end fragment bottomLeft to linebox topleft
    // 3. Convert both topleft/bottomright to logical, so that we can
    // 4. Enforce logical topLeft < bottomRight
    // 5. Compute size, physical offset
    const NGPhysicalFragment* start_fragment = block_info.value.start_fragment;
    const NGPhysicalLineBoxFragment* start_linebox_fragment =
        block_info.value.start_linebox_fragment;
    WritingMode container_writing_mode = container_style_.GetWritingMode();
    TextDirection container_direction = container_style_.Direction();
    // Step 1
    NGLogicalOffset start_fragment_logical_offset =
        block_info.value.start_fragment_offset.ConvertToLogical(
            container_writing_mode, container_direction,
            start_linebox_fragment->Size(), start_fragment->Size());
    // Text fragments do not include inline-cb borders and padding.
    if (start_fragment->IsText()) {
      start_fragment_logical_offset -= inline_cb_borders.StartOffset();
      start_fragment_logical_offset -= inline_cb_padding.StartOffset();
    }
    NGPhysicalOffset start_fragment_physical_offset =
        start_fragment_logical_offset.ConvertToPhysical(
            container_writing_mode, container_direction,
            start_linebox_fragment->Size(), NGPhysicalSize());
    NGPhysicalOffset start_linebox_physical_offset =
        block_info.value.start_linebox_offset.ConvertToPhysical(
            container_writing_mode, container_direction,
            container_builder_physical_size, start_linebox_fragment->Size());
    start_fragment_physical_offset += start_linebox_physical_offset;
    // Step 2
    const NGPhysicalLineBoxFragment* end_linebox_fragment =
        block_info.value.end_linebox_fragment;
    const NGPhysicalFragment* end_fragment = block_info.value.end_fragment;
    NGLogicalOffset end_fragment_logical_offset =
        block_info.value.end_fragment_offset.ConvertToLogical(
            container_writing_mode, container_direction,
            end_linebox_fragment->Size(), end_fragment->Size());
    // Text fragments do not include inline-cb borders and padding.
    if (end_fragment->IsText()) {
      end_fragment_logical_offset += NGLogicalOffset(
          inline_cb_borders.inline_end, inline_cb_borders.block_end);
      end_fragment_logical_offset += NGLogicalOffset(
          inline_cb_padding.inline_end, inline_cb_padding.block_end);
    }
    NGLogicalOffset end_fragment_bottom_right =
        end_fragment_logical_offset +
        end_fragment->Size().ConvertToLogical(container_writing_mode);
    NGPhysicalOffset end_fragment_physical_offset =
        end_fragment_bottom_right.ConvertToPhysical(
            container_writing_mode, container_direction,
            end_linebox_fragment->Size(), NGPhysicalSize());
    NGPhysicalOffset end_linebox_physical_offset =
        block_info.value.end_linebox_offset.ConvertToPhysical(
            container_writing_mode, container_direction,
            container_builder_physical_size, end_linebox_fragment->Size());
    end_fragment_physical_offset += end_linebox_physical_offset;
    // Step 3
    NGLogicalOffset start_fragment_logical_offset_wrt_box =
        start_fragment_physical_offset.ConvertToLogical(
            inline_cb_style->GetWritingMode(), inline_cb_style->Direction(),
            container_builder_physical_size, NGPhysicalSize());
    NGLogicalOffset end_fragment_logical_offset_wrt_box =
        end_fragment_physical_offset.ConvertToLogical(
            inline_cb_style->GetWritingMode(), inline_cb_style->Direction(),
            container_builder_physical_size, NGPhysicalSize());

    // Step 4
    end_fragment_logical_offset_wrt_box.inline_offset =
        std::max(end_fragment_logical_offset_wrt_box.inline_offset,
                 start_fragment_logical_offset_wrt_box.inline_offset +
                     inline_cb_borders.InlineSum());
    end_fragment_logical_offset_wrt_box.block_offset =
        std::max(end_fragment_logical_offset_wrt_box.block_offset,
                 start_fragment_logical_offset_wrt_box.block_offset +
                     inline_cb_borders.BlockSum());

    // Step 5
    inline_cb_size.inline_size =
        end_fragment_logical_offset_wrt_box.inline_offset -
        start_fragment_logical_offset_wrt_box.inline_offset -
        inline_cb_borders.InlineSum();
    inline_cb_size.block_size =
        end_fragment_logical_offset_wrt_box.block_offset -
        start_fragment_logical_offset_wrt_box.block_offset -
        inline_cb_borders.BlockSum();

    DCHECK_GE(inline_cb_size.inline_size, LayoutUnit());
    DCHECK_GE(inline_cb_size.block_size, LayoutUnit());

    inline_content_offset = NGLogicalOffset{inline_cb_borders.inline_start,
                                            inline_cb_borders.block_start};
    inline_content_physical_offset =
        NGPhysicalOffset(physical_borders.left, physical_borders.top);

    NGLogicalOffset default_container_offset;
    if (RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled()) {
      // NGPaint offset is wrt parent fragment.
      default_container_offset =
          start_fragment_logical_offset - content_offset_;
      default_container_offset += NGLogicalOffset(
          inline_cb_borders.inline_start, inline_cb_borders.block_start);
    } else {
      // Legacy offset is wrt inline container.
      default_container_offset =
          NGLogicalOffset(inline_cb_borders.inline_start,
                          inline_cb_borders.block_start) -
          content_offset_;
    }

    containing_blocks_map_.insert(
        block_info.key, ContainingBlockInfo{inline_cb_style, inline_cb_size,
                                            inline_content_offset,
                                            inline_content_physical_offset,
                                            default_container_offset});
  }
}

scoped_refptr<NGLayoutResult> NGOutOfFlowLayoutPart::LayoutDescendant(
    const NGOutOfFlowPositionedDescendant& descendant,
    NGLogicalOffset* offset) {
  ContainingBlockInfo container_info = GetContainingBlockInfo(descendant);

  WritingMode container_writing_mode(container_info.style->GetWritingMode());
  WritingMode descendant_writing_mode(descendant.node.Style().GetWritingMode());

  // Adjust the static_position origin. The static_position coordinate origin is
  // relative to the container's border box, ng_absolute_utils expects it to be
  // relative to the container's padding box.
  NGStaticPosition static_position(descendant.static_position);
  static_position.offset -= container_info.content_physical_offset;

  // The block estimate is in the descendant's writing mode.
  scoped_refptr<NGConstraintSpace> descendant_constraint_space =
      NGConstraintSpaceBuilder(container_writing_mode, icb_size_)
          .SetTextDirection(container_info.style->Direction())
          .SetAvailableSize(container_info.content_size)
          .SetPercentageResolutionSize(container_info.content_size)
          .ToConstraintSpace(descendant_writing_mode);
  Optional<MinMaxSize> min_max_size;
  Optional<LayoutUnit> block_estimate;

  scoped_refptr<NGLayoutResult> layout_result = nullptr;

  NGBlockNode node = descendant.node;
  if (AbsoluteNeedsChildInlineSize(descendant.node.Style()) ||
      NeedMinMaxSize(descendant.node.Style())) {
    min_max_size = node.ComputeMinMaxSize();
  }

  Optional<NGLogicalSize> replaced_size;
  if (descendant.node.IsReplaced())
    replaced_size = ComputeReplacedSize(
        descendant.node, *descendant_constraint_space, min_max_size);

  NGAbsolutePhysicalPosition node_position =
      ComputePartialAbsoluteWithChildInlineSize(
          *descendant_constraint_space, descendant.node.Style(),
          static_position, min_max_size, replaced_size, container_writing_mode,
          container_info.style->Direction());

  if (AbsoluteNeedsChildBlockSize(descendant.node.Style())) {
    layout_result = GenerateFragment(descendant.node, container_info,
                                     block_estimate, node_position);

    DCHECK(layout_result->PhysicalFragment().get());
    NGFragment fragment(descendant_writing_mode,
                        *layout_result->PhysicalFragment());

    block_estimate = fragment.BlockSize();
  }

  ComputeFullAbsoluteWithChildBlockSize(
      *descendant_constraint_space, descendant.node.Style(), static_position,
      block_estimate, replaced_size, container_writing_mode,
      container_info.style->Direction(), &node_position);

  // Skip this step if we produced a fragment when estimating the block size.
  if (!layout_result) {
    block_estimate =
        node_position.size.ConvertToLogical(descendant_writing_mode).block_size;
    layout_result = GenerateFragment(descendant.node, container_info,
                                     block_estimate, node_position);
  }

  // Compute logical offset, NGAbsolutePhysicalPosition is calculated relative
  // to the padding box so add back the container's borders.
  NGBoxStrut inset = node_position.inset.ConvertToLogical(
      container_writing_mode, container_info.style->Direction());
  offset->inline_offset = inset.inline_start + content_offset_.inline_offset;
  offset->block_offset = inset.block_start + content_offset_.block_offset;

  offset->inline_offset +=
      container_info.default_container_offset.inline_offset;
  offset->block_offset += container_info.default_container_offset.block_offset;
  return layout_result;
}

bool NGOutOfFlowLayoutPart::IsContainingBlockForDescendant(
    const NGOutOfFlowPositionedDescendant& descendant) {
  EPosition position = descendant.node.Style().GetPosition();
  if (descendant.inline_container) {
    DCHECK(position == EPosition::kAbsolute);
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
    const Optional<LayoutUnit>& block_estimate,
    const NGAbsolutePhysicalPosition node_position) {
  // As the block_estimate is always in the descendant's writing mode, we build
  // the constraint space in the descendant's writing mode.
  WritingMode writing_mode(descendant.Style().GetWritingMode());
  NGLogicalSize container_size(
      container_info.content_size
          .ConvertToPhysical(container_style_.GetWritingMode())
          .ConvertToLogical(writing_mode));

  LayoutUnit inline_size =
      node_position.size.ConvertToLogical(writing_mode).inline_size;
  LayoutUnit block_size =
      block_estimate ? *block_estimate : container_size.block_size;

  NGLogicalSize available_size{inline_size, block_size};

  // TODO(atotic) will need to be adjusted for scrollbars.
  NGConstraintSpaceBuilder builder(writing_mode, icb_size_);
  builder.SetAvailableSize(available_size)
      .SetTextDirection(descendant.Style().Direction())
      .SetPercentageResolutionSize(container_size)
      .SetIsNewFormattingContext(true)
      .SetIsFixedSizeInline(true);
  if (block_estimate)
    builder.SetIsFixedSizeBlock(true);
  scoped_refptr<NGConstraintSpace> space =
      builder.ToConstraintSpace(writing_mode);

  return descendant.Layout(*space);
}

}  // namespace blink
