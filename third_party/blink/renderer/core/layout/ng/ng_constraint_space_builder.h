// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpaceBuilder_h
#define NGConstraintSpaceBuilder_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class NGExclusionSpace;

class CORE_EXPORT NGConstraintSpaceBuilder final {
  STACK_ALLOCATED();

 public:
  // NOTE: This constructor doesn't act like a copy-constructor, it uses the
  // writing_mode and icb_size from the parent constraint space, and passes
  // them to the constructor below.
  NGConstraintSpaceBuilder(const NGConstraintSpace& parent_space);

  NGConstraintSpaceBuilder(WritingMode writing_mode, NGPhysicalSize icb_size);

  NGConstraintSpaceBuilder& SetAvailableSize(NGLogicalSize available_size);

  NGConstraintSpaceBuilder& SetPercentageResolutionSize(NGLogicalSize);

  NGConstraintSpaceBuilder& SetReplacedPercentageResolutionSize(NGLogicalSize);

  NGConstraintSpaceBuilder& SetFragmentainerBlockSize(LayoutUnit size) {
    fragmentainer_block_size_ = size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerSpaceAtBfcStart(LayoutUnit space) {
    fragmentainer_space_at_bfc_start_ = space;
    return *this;
  }

  NGConstraintSpaceBuilder& SetTextDirection(TextDirection);

  NGConstraintSpaceBuilder& SetIsFixedSizeInline(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeInline, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsFixedSizeBlock(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeBlock, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetFixedSizeBlockIsDefinite(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeBlockIsDefinite, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsShrinkToFit(bool b) {
    SetFlag(NGConstraintSpace::kShrinkToFit, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsIntermediateLayout(bool b) {
    SetFlag(NGConstraintSpace::kIntermediateLayout, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentationType(NGFragmentationType);

  NGConstraintSpaceBuilder& SetSeparateLeadingFragmentainerMargins(bool b) {
    SetFlag(NGConstraintSpace::kSeparateLeadingFragmentainerMargins, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsNewFormattingContext(bool b) {
    SetFlag(NGConstraintSpace::kNewFormattingContext, b);
    return *this;
  }
  NGConstraintSpaceBuilder& SetIsAnonymous(bool b) {
    SetFlag(NGConstraintSpace::kAnonymous, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetUseFirstLineStyle(bool b) {
    SetFlag(NGConstraintSpace::kUseFirstLineStyle, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetAdjoiningFloatTypes(NGFloatTypes floats) {
    adjoining_floats_ = floats;
    return *this;
  }

  NGConstraintSpaceBuilder& SetMarginStrut(const NGMarginStrut& margin_strut);

  NGConstraintSpaceBuilder& SetBfcOffset(const NGBfcOffset& bfc_offset);
  NGConstraintSpaceBuilder& SetFloatsBfcBlockOffset(
      const base::Optional<LayoutUnit>&);

  NGConstraintSpaceBuilder& SetClearanceOffset(LayoutUnit clearance_offset);

  NGConstraintSpaceBuilder& SetShouldForceClearance(bool b) {
    SetFlag(NGConstraintSpace::kForceClearance, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetTableCellChildLayoutPhase(
      NGTableCellChildLayoutPhase table_cell_child_layout_phase) {
    table_cell_child_layout_phase_ = table_cell_child_layout_phase;
    return *this;
  }

  NGConstraintSpaceBuilder& SetExclusionSpace(
      const NGExclusionSpace& exclusion_space);

  void AddBaselineRequests(const Vector<NGBaselineRequest>&);
  NGConstraintSpaceBuilder& AddBaselineRequest(const NGBaselineRequest&);

  // Creates a new constraint space. This may be called multiple times, for
  // example the constraint space will be different for a child which:
  //  - Establishes a new formatting context.
  //  - Is within a fragmentation container and needs its fragmentation offset
  //    updated.
  //  - Has its size is determined by its parent layout (flex, abs-pos).
  //
  // WritingMode specifies the writing mode of the generated space.
  const NGConstraintSpace ToConstraintSpace(WritingMode);

 private:
  void SetFlag(NGConstraintSpace::ConstraintSpaceFlags mask, bool value) {
    flags_ = (flags_ & ~static_cast<unsigned>(mask)) |
             (-(int32_t)value & static_cast<unsigned>(mask));
  }

  // NOTE: The below NGLogicalSizes are relative to parent_writing_mode_.
  NGLogicalSize available_size_;
  NGLogicalSize percentage_resolution_size_;
  NGLogicalSize replaced_percentage_resolution_size_;

  base::Optional<NGLogicalSize> parent_percentage_resolution_size_;
  NGPhysicalSize initial_containing_block_size_;
  LayoutUnit fragmentainer_block_size_ = NGSizeIndefinite;
  LayoutUnit fragmentainer_space_at_bfc_start_ = NGSizeIndefinite;

  WritingMode parent_writing_mode_;
  NGFragmentationType fragmentation_type_ = kFragmentNone;
  NGTableCellChildLayoutPhase table_cell_child_layout_phase_ =
      kNotTableCellChild;
  NGFloatTypes adjoining_floats_ = kFloatTypeNone;
  TextDirection text_direction_ = TextDirection::kLtr;

  unsigned flags_;

  NGMarginStrut margin_strut_;
  NGBfcOffset bfc_offset_;
  base::Optional<LayoutUnit> floats_bfc_block_offset_;
  const NGExclusionSpace* exclusion_space_ = nullptr;
  LayoutUnit clearance_offset_;
  Vector<NGBaselineRequest> baseline_requests_;
};

}  // namespace blink

#endif  // NGConstraintSpaceBuilder
