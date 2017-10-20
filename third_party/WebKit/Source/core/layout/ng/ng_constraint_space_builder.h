// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpaceBuilder_h
#define NGConstraintSpaceBuilder_h

#include "core/layout/ng/geometry/ng_bfc_offset.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_exclusion.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"

namespace blink {

class CORE_EXPORT NGConstraintSpaceBuilder final {
  DISALLOW_NEW();

 public:
  // NOTE: This constructor doesn't act like a copy-constructor, it uses the
  // writing_mode and icb_size from the parent constraint space, and passes
  // them to the constructor below.
  NGConstraintSpaceBuilder(const NGConstraintSpace& parent_space);

  NGConstraintSpaceBuilder(NGWritingMode writing_mode, NGPhysicalSize icb_size);

  NGConstraintSpaceBuilder& SetAvailableSize(NGLogicalSize available_size);

  NGConstraintSpaceBuilder& SetPercentageResolutionSize(
      NGLogicalSize percentage_resolution_size);

  NGConstraintSpaceBuilder& SetFragmentainerBlockSize(LayoutUnit size) {
    fragmentainer_block_size_ = size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerSpaceAtBfcStart(LayoutUnit space) {
    fragmentainer_space_at_bfc_start_ = space;
    return *this;
  }

  NGConstraintSpaceBuilder& SetTextDirection(TextDirection);

  NGConstraintSpaceBuilder& SetIsFixedSizeInline(bool is_fixed_size_inline);
  NGConstraintSpaceBuilder& SetIsFixedSizeBlock(bool is_fixed_size_block);

  NGConstraintSpaceBuilder& SetIsShrinkToFit(bool shrink_to_fit);

  NGConstraintSpaceBuilder& SetIsInlineDirectionTriggersScrollbar(
      bool is_inline_direction_triggers_scrollbar);
  NGConstraintSpaceBuilder& SetIsBlockDirectionTriggersScrollbar(
      bool is_block_direction_triggers_scrollbar);

  NGConstraintSpaceBuilder& SetFragmentationType(NGFragmentationType);
  NGConstraintSpaceBuilder& SetIsNewFormattingContext(bool is_new_fc);
  NGConstraintSpaceBuilder& SetIsAnonymous(bool is_anonymous);
  NGConstraintSpaceBuilder& SetUseFirstLineStyle(bool use_first_line_sytle);

  NGConstraintSpaceBuilder& SetUnpositionedFloats(
      Vector<scoped_refptr<NGUnpositionedFloat>>& unpositioned_floats);

  NGConstraintSpaceBuilder& SetMarginStrut(const NGMarginStrut& margin_strut);

  NGConstraintSpaceBuilder& SetBfcOffset(const NGBfcOffset& bfc_offset);
  NGConstraintSpaceBuilder& SetFloatsBfcOffset(
      const WTF::Optional<NGBfcOffset>& floats_bfc_offset);

  NGConstraintSpaceBuilder& SetClearanceOffset(
      const WTF::Optional<LayoutUnit>& clearance_offset);

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
  // NGWritingMode specifies the writing mode of the generated space.
  scoped_refptr<NGConstraintSpace> ToConstraintSpace(NGWritingMode);

 private:
  // Relative to parent_writing_mode_.
  NGLogicalSize available_size_;
  // Relative to parent_writing_mode_.
  NGLogicalSize percentage_resolution_size_;
  Optional<NGLogicalSize> parent_percentage_resolution_size_;
  NGPhysicalSize initial_containing_block_size_;
  LayoutUnit fragmentainer_block_size_;
  LayoutUnit fragmentainer_space_at_bfc_start_;

  unsigned parent_writing_mode_ : 3;
  unsigned is_fixed_size_inline_ : 1;
  unsigned is_fixed_size_block_ : 1;
  unsigned is_shrink_to_fit_ : 1;
  unsigned is_inline_direction_triggers_scrollbar_ : 1;
  unsigned is_block_direction_triggers_scrollbar_ : 1;
  unsigned fragmentation_type_ : 2;
  unsigned is_new_fc_ : 1;
  unsigned is_anonymous_ : 1;
  unsigned use_first_line_sytle_ : 1;
  unsigned text_direction_ : 1;

  NGMarginStrut margin_strut_;
  NGBfcOffset bfc_offset_;
  WTF::Optional<NGBfcOffset> floats_bfc_offset_;
  const NGExclusionSpace* exclusion_space_;
  WTF::Optional<LayoutUnit> clearance_offset_;
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats_;
  Vector<NGBaselineRequest> baseline_requests_;
};

}  // namespace blink

#endif  // NGConstraintSpaceBuilder
