// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space_builder.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(NGWritingMode writing_mode)
    : writing_mode_(writing_mode) {}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetContainerSize(
    NGLogicalSize container_size) {
  container_size_ = container_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFixedSize(
    bool fixed_inline,
    bool fixed_block) {
  fixed_inline_ = fixed_inline;
  fixed_block_ = fixed_block;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetOverflowTriggersScrollbar(
    bool inline_direction_triggers_scrollbar,
    bool block_direction_triggers_scrollbar) {
  inline_direction_triggers_scrollbar_ = inline_direction_triggers_scrollbar;
  block_direction_triggers_scrollbar_ = block_direction_triggers_scrollbar;
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

NGPhysicalConstraintSpace* NGConstraintSpaceBuilder::ToConstraintSpace() {
  NGPhysicalSize container_size = container_size_.ConvertToPhysical(
      static_cast<NGWritingMode>(writing_mode_));
  if (writing_mode_ == HorizontalTopBottom) {
    return new NGPhysicalConstraintSpace(
        container_size, fixed_inline_, fixed_block_,
        inline_direction_triggers_scrollbar_,
        block_direction_triggers_scrollbar_, FragmentNone,
        static_cast<NGFragmentationType>(fragmentation_type_), is_new_fc_);
  } else {
    return new NGPhysicalConstraintSpace(
        container_size, fixed_block_, fixed_inline_,
        block_direction_triggers_scrollbar_,
        inline_direction_triggers_scrollbar_,
        static_cast<NGFragmentationType>(fragmentation_type_), FragmentNone,
        is_new_fc_);
  }
}

}  // namespace blink
