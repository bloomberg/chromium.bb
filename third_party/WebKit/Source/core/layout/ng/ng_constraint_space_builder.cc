// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space_builder.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(
    const NGConstraintSpace* parent_space)
    : available_size_(parent_space->AvailableSize()),
      percentage_resolution_size_(parent_space->PercentageResolutionSize()),
      writing_mode_(parent_space->WritingMode()),
      parent_writing_mode_(writing_mode_),
      is_fixed_size_inline_(false),
      is_fixed_size_block_(false),
      is_inline_direction_triggers_scrollbar_(false),
      is_block_direction_triggers_scrollbar_(false),
      fragmentation_type_(kFragmentNone),
      is_new_fc_(parent_space->IsNewFormattingContext()),
      text_direction_(parent_space->Direction()),
      exclusions_(parent_space->Exclusions()) {}

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(NGWritingMode writing_mode)
    : writing_mode_(writing_mode),
      parent_writing_mode_(writing_mode_),
      is_fixed_size_inline_(false),
      is_fixed_size_block_(false),
      is_inline_direction_triggers_scrollbar_(false),
      is_block_direction_triggers_scrollbar_(false),
      fragmentation_type_(kFragmentNone),
      is_new_fc_(false),
      text_direction_(TextDirection::LTR),
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

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetTextDirection(
    TextDirection text_direction) {
  text_direction_ = text_direction;
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

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetWritingMode(
    NGWritingMode writing_mode) {
  writing_mode_ = writing_mode;
  return *this;
}

NGConstraintSpace* NGConstraintSpaceBuilder::ToConstraintSpace() {
  // Exclusions do not pass the formatting context boundary.
  std::shared_ptr<NGExclusions> exclusions(
      is_new_fc_ ? std::make_shared<NGExclusions>() : exclusions_);

  // Whether the child and the containing block are parallel to each other.
  // Example: vertical-rl and vertical-lr
  bool is_in_parallel_flow = (parent_writing_mode_ == kHorizontalTopBottom) ==
                             (writing_mode_ == kHorizontalTopBottom);

  if (is_in_parallel_flow) {
    return new NGConstraintSpace(
        static_cast<NGWritingMode>(writing_mode_),
        static_cast<TextDirection>(text_direction_),
        {available_size_.inline_size, available_size_.block_size},
        {percentage_resolution_size_.inline_size,
         percentage_resolution_size_.block_size},
        is_fixed_size_inline_, is_fixed_size_block_,
        is_inline_direction_triggers_scrollbar_,
        is_block_direction_triggers_scrollbar_,
        static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_,
        exclusions);
  }

  return new NGConstraintSpace(
      static_cast<NGWritingMode>(writing_mode_),
      static_cast<TextDirection>(text_direction_),
      {available_size_.block_size, available_size_.inline_size},
      {percentage_resolution_size_.block_size,
       percentage_resolution_size_.inline_size},
      is_fixed_size_block_, is_fixed_size_inline_,
      is_block_direction_triggers_scrollbar_,
      is_inline_direction_triggers_scrollbar_,
      static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_,
      exclusions);
}

}  // namespace blink
