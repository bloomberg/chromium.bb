// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space_builder.h"

#include "core/layout/ng/ng_length_utils.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(
    const NGConstraintSpace* parent_space)
    : available_size_(parent_space->AvailableSize()),
      percentage_resolution_size_(parent_space->PercentageResolutionSize()),
      initial_containing_block_size_(
          parent_space->InitialContainingBlockSize()),
      fragmentainer_space_available_(NGSizeIndefinite),
      parent_writing_mode_(parent_space->WritingMode()),
      is_fixed_size_inline_(false),
      is_fixed_size_block_(false),
      is_shrink_to_fit_(false),
      is_inline_direction_triggers_scrollbar_(false),
      is_block_direction_triggers_scrollbar_(false),
      fragmentation_type_(parent_space->BlockFragmentationType()),
      is_new_fc_(parent_space->IsNewFormattingContext()),
      is_anonymous_(false),
      text_direction_(static_cast<unsigned>(parent_space->Direction())),
      bfc_offset_(parent_space->bfc_offset_),
      exclusions_(parent_space->Exclusions()) {}

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(NGWritingMode writing_mode)
    : initial_containing_block_size_{NGSizeIndefinite, NGSizeIndefinite},
      fragmentainer_space_available_(NGSizeIndefinite),
      parent_writing_mode_(writing_mode),
      is_fixed_size_inline_(false),
      is_fixed_size_block_(false),
      is_shrink_to_fit_(false),
      is_inline_direction_triggers_scrollbar_(false),
      is_block_direction_triggers_scrollbar_(false),
      fragmentation_type_(kFragmentNone),
      is_new_fc_(false),
      is_anonymous_(false),
      text_direction_(static_cast<unsigned>(TextDirection::kLtr)),
      exclusions_(new NGExclusions()) {}

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
NGConstraintSpaceBuilder::SetInitialContainingBlockSize(NGPhysicalSize size) {
  initial_containing_block_size_ = size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetTextDirection(
    TextDirection text_direction) {
  text_direction_ = static_cast<unsigned>(text_direction);
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetMarginStrut(
    const NGMarginStrut& margin_strut) {
  margin_strut_ = margin_strut;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetBfcOffset(
    const NGLogicalOffset& bfc_offset) {
  bfc_offset_ = bfc_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFloatsBfcOffset(
    const WTF::Optional<NGLogicalOffset>& floats_bfc_offset) {
  floats_bfc_offset_ = floats_bfc_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetClearanceOffset(
    const WTF::Optional<LayoutUnit>& clearance_offset) {
  clearance_offset_ = clearance_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsFixedSizeInline(
    bool is_fixed_size_inline) {
  is_fixed_size_inline_ = is_fixed_size_inline;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsFixedSizeBlock(
    bool is_fixed_size_block) {
  is_fixed_size_block_ = is_fixed_size_block;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsShrinkToFit(
    bool shrink_to_fit) {
  is_shrink_to_fit_ = shrink_to_fit;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetIsInlineDirectionTriggersScrollbar(
    bool is_inline_direction_triggers_scrollbar) {
  is_inline_direction_triggers_scrollbar_ =
      is_inline_direction_triggers_scrollbar;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetIsBlockDirectionTriggersScrollbar(
    bool is_block_direction_triggers_scrollbar) {
  is_block_direction_triggers_scrollbar_ =
      is_block_direction_triggers_scrollbar;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFragmentationType(
    NGFragmentationType fragmentation_type) {
  fragmentation_type_ = fragmentation_type;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsNewFormattingContext(
    bool is_new_fc) {
  is_new_fc_ = is_new_fc;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetIsAnonymous(
    bool is_anonymous) {
  is_anonymous_ = is_anonymous;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetUnpositionedFloats(
    Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats) {
  unpositioned_floats_.swap(unpositioned_floats);
  return *this;
}

RefPtr<NGConstraintSpace> NGConstraintSpaceBuilder::ToConstraintSpace(
    NGWritingMode out_writing_mode) {
  // Whether the child and the containing block are parallel to each other.
  // Example: vertical-rl and vertical-lr
  bool is_in_parallel_flow = IsParallelWritingMode(
      static_cast<NGWritingMode>(parent_writing_mode_), out_writing_mode);

  NGLogicalSize available_size = available_size_;
  NGLogicalSize percentage_resolution_size = percentage_resolution_size_;
  if (!is_in_parallel_flow) {
    std::swap(available_size.inline_size, available_size.block_size);
    std::swap(percentage_resolution_size.inline_size,
              percentage_resolution_size.block_size);
  }

  // If inline size is indefinite, use size of initial containing block.
  // https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
  if (available_size.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == kHorizontalTopBottom) {
      available_size.inline_size = initial_containing_block_size_.width;
    } else {
      available_size.inline_size = initial_containing_block_size_.height;
    }
  }
  if (percentage_resolution_size.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == kHorizontalTopBottom) {
      percentage_resolution_size.inline_size =
          initial_containing_block_size_.width;
    } else {
      percentage_resolution_size.inline_size =
          initial_containing_block_size_.height;
    }
  }

  // Reset things that do not pass the Formatting Context boundary.
  std::shared_ptr<NGExclusions> exclusions(
      is_new_fc_ ? std::make_shared<NGExclusions>() : exclusions_);
  if (is_new_fc_)
    DCHECK(unpositioned_floats_.IsEmpty());
  NGLogicalOffset bfc_offset = is_new_fc_ ? NGLogicalOffset() : bfc_offset_;
  NGMarginStrut margin_strut = is_new_fc_ ? NGMarginStrut() : margin_strut_;
  WTF::Optional<LayoutUnit> clearance_offset =
      is_new_fc_ ? WTF::nullopt : clearance_offset_;
  WTF::Optional<NGLogicalOffset> floats_bfc_offset =
      is_new_fc_ ? WTF::nullopt : floats_bfc_offset_;

  // The inline_offsets of the bfc_offset and floats_bfc_offset should match.
  if (floats_bfc_offset)
    DCHECK_EQ(bfc_offset.inline_offset,
              floats_bfc_offset.value().inline_offset);

  if (is_in_parallel_flow) {
    return AdoptRef(new NGConstraintSpace(
        static_cast<NGWritingMode>(out_writing_mode),
        static_cast<TextDirection>(text_direction_), available_size,
        percentage_resolution_size, initial_containing_block_size_,
        fragmentainer_space_available_, is_fixed_size_inline_,
        is_fixed_size_block_, is_shrink_to_fit_,
        is_inline_direction_triggers_scrollbar_,
        is_block_direction_triggers_scrollbar_,
        static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_,
        is_anonymous_, margin_strut, bfc_offset, floats_bfc_offset, exclusions,
        unpositioned_floats_, clearance_offset));
  }
  return AdoptRef(new NGConstraintSpace(
      out_writing_mode, static_cast<TextDirection>(text_direction_),
      available_size, percentage_resolution_size,
      initial_containing_block_size_, fragmentainer_space_available_,
      is_fixed_size_block_, is_fixed_size_inline_, is_shrink_to_fit_,
      is_block_direction_triggers_scrollbar_,
      is_inline_direction_triggers_scrollbar_,
      static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_,
      is_anonymous_, margin_strut, bfc_offset, floats_bfc_offset, exclusions,
      unpositioned_floats_, clearance_offset));
}

}  // namespace blink
