// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(
    const NGConstraintSpace& parent_space)
    : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                               parent_space.InitialContainingBlockSize()) {
  parent_percentage_resolution_size_ = parent_space.PercentageResolutionSize();

  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
  if (parent_space.IsIntermediateLayout())
    flags_ |= NGConstraintSpace::kIntermediateLayout;
}

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(WritingMode writing_mode,
                                                   NGPhysicalSize icb_size)
    : initial_containing_block_size_(icb_size),
      parent_writing_mode_(writing_mode) {
  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetAvailableSize(
    NGLogicalSize available_size) {
  available_size_ = available_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetPercentageResolutionSize(
    NGLogicalSize percentage_resolution_size) {
  percentage_resolution_size_ = percentage_resolution_size;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetReplacedPercentageResolutionSize(
    NGLogicalSize replaced_percentage_resolution_size) {
  replaced_percentage_resolution_size_ = replaced_percentage_resolution_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetTextDirection(
    TextDirection text_direction) {
  text_direction_ = text_direction;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetMarginStrut(
    const NGMarginStrut& margin_strut) {
  margin_strut_ = margin_strut;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetBfcOffset(
    const NGBfcOffset& bfc_offset) {
  bfc_offset_ = bfc_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFloatsBfcBlockOffset(
    const base::Optional<LayoutUnit>& floats_bfc_block_offset) {
  floats_bfc_block_offset_ = floats_bfc_block_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetClearanceOffset(
    LayoutUnit clearance_offset) {
  clearance_offset_ = clearance_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetExclusionSpace(
    const NGExclusionSpace& exclusion_space) {
  exclusion_space_ = &exclusion_space;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFragmentationType(
    NGFragmentationType fragmentation_type) {
  fragmentation_type_ = fragmentation_type;
  return *this;
}

void NGConstraintSpaceBuilder::AddBaselineRequests(
    const Vector<NGBaselineRequest>& requests) {
  DCHECK(baseline_requests_.IsEmpty());
  baseline_requests_.AppendVector(requests);
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::AddBaselineRequest(
    const NGBaselineRequest& request) {
  for (const auto& existing : baseline_requests_) {
    if (existing == request)
      return *this;
  }
  baseline_requests_.push_back(request);
  return *this;
}

const NGConstraintSpace NGConstraintSpaceBuilder::ToConstraintSpace(
    WritingMode out_writing_mode) {
  // Whether the child and the containing block are parallel to each other.
  // Example: vertical-rl and vertical-lr
  bool is_in_parallel_flow =
      IsParallelWritingMode(parent_writing_mode_, out_writing_mode);

  NGLogicalSize available_size = available_size_;
  NGLogicalSize percentage_resolution_size = percentage_resolution_size_;
  NGLogicalSize replaced_percentage_resolution_size =
      replaced_percentage_resolution_size_;
  NGLogicalSize parent_percentage_resolution_size =
      parent_percentage_resolution_size_.value_or(percentage_resolution_size);

  unsigned resolved_flags = flags_;
  auto SetResolvedFlag = [&resolved_flags](unsigned mask, bool value) {
    resolved_flags = (resolved_flags & ~static_cast<unsigned>(mask)) |
                     (-(int32_t)value & static_cast<unsigned>(mask));
  };
  if (!is_in_parallel_flow) {
    available_size.Flip();
    percentage_resolution_size.Flip();
    replaced_percentage_resolution_size.Flip();
    parent_percentage_resolution_size.Flip();
    SetResolvedFlag(NGConstraintSpace::kFixedSizeInline,
                    flags_ & NGConstraintSpace::kFixedSizeBlock);
    SetResolvedFlag(NGConstraintSpace::kFixedSizeBlock,
                    flags_ & NGConstraintSpace::kFixedSizeInline);
    SetResolvedFlag(NGConstraintSpace::kFixedSizeBlockIsDefinite, true);
    SetResolvedFlag(NGConstraintSpace::kOrthogonalWritingModeRoot, true);
  }
  DCHECK_EQ(resolved_flags & NGConstraintSpace::kOrthogonalWritingModeRoot,
            !is_in_parallel_flow);

  // If inline size is indefinite, use size of initial containing block.
  // https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
  if (available_size.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      available_size.inline_size = initial_containing_block_size_.width;
    } else {
      available_size.inline_size = initial_containing_block_size_.height;
    }
  }
  if (percentage_resolution_size.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      percentage_resolution_size.inline_size =
          initial_containing_block_size_.width;
    } else {
      percentage_resolution_size.inline_size =
          initial_containing_block_size_.height;
    }
  }
  if (replaced_percentage_resolution_size.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      replaced_percentage_resolution_size.inline_size =
          initial_containing_block_size_.width;
    } else {
      replaced_percentage_resolution_size.inline_size =
          initial_containing_block_size_.height;
    }
  }

  bool is_new_fc_ = flags_ & NGConstraintSpace::kNewFormattingContext;
  DCHECK(!is_new_fc_ || !adjoining_floats_);

  const NGExclusionSpace& exclusion_space = (is_new_fc_ || !exclusion_space_)
                                                ? NGExclusionSpace()
                                                : *exclusion_space_;
  NGBfcOffset bfc_offset = is_new_fc_ ? NGBfcOffset() : bfc_offset_;
  NGMarginStrut margin_strut = is_new_fc_ ? NGMarginStrut() : margin_strut_;
  LayoutUnit clearance_offset =
      is_new_fc_ ? LayoutUnit::Min() : clearance_offset_;
  base::Optional<LayoutUnit> floats_bfc_block_offset =
      is_new_fc_ ? base::nullopt : floats_bfc_block_offset_;

  return NGConstraintSpace(
      out_writing_mode, text_direction_, available_size,
      percentage_resolution_size, replaced_percentage_resolution_size,
      parent_percentage_resolution_size.inline_size,
      initial_containing_block_size_, fragmentainer_block_size_,
      fragmentainer_space_at_bfc_start_, fragmentation_type_,
      table_cell_child_layout_phase_, adjoining_floats_, margin_strut,
      bfc_offset, floats_bfc_block_offset, exclusion_space, clearance_offset,
      baseline_requests_, resolved_flags);
}

}  // namespace blink
