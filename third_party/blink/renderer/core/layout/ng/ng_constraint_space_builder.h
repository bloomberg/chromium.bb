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
  // The setters on this builder are in the writing mode of parent_space.
  NGConstraintSpaceBuilder(const NGConstraintSpace& parent_space,
                           WritingMode out_writing_mode,
                           bool is_new_fc)
      : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                                 out_writing_mode,
                                 parent_space.InitialContainingBlockSize(),
                                 is_new_fc) {
    // Propagate the intermediate layout bit to the child constraint space.
    if (parent_space.IsIntermediateLayout())
      space_.flags_ |= NGConstraintSpace::kIntermediateLayout;
  }

  // The setters on this builder are in the writing mode of parent_writing_mode.
  //
  // forced_orthogonal_writing_mode_root is set for constraint spaces created
  // directly from a LayoutObject. In this case parent_writing_mode isn't
  // actually the parent's, it's the same as out_writing_mode.
  // When this occurs we would miss setting the kOrthogonalWritingModeRoot flag
  // unless we force it.
  NGConstraintSpaceBuilder(WritingMode parent_writing_mode,
                           WritingMode out_writing_mode,
                           const NGPhysicalSize& icb_size,
                           bool is_new_fc,
                           bool force_orthogonal_writing_mode_root = false)
      : space_(out_writing_mode, icb_size),
        is_in_parallel_flow_(
            IsParallelWritingMode(parent_writing_mode, out_writing_mode)),
        is_new_fc_(is_new_fc),
        force_orthogonal_writing_mode_root_(
            force_orthogonal_writing_mode_root) {
    if (is_new_fc_)
      space_.flags_ |= NGConstraintSpace::kNewFormattingContext;

    if (!is_in_parallel_flow_ || force_orthogonal_writing_mode_root_)
      space_.flags_ |= NGConstraintSpace::kOrthogonalWritingModeRoot;
  }

  // If inline size is indefinite, use size of initial containing block.
  // https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
  void AdjustInlineSizeIfNeeded(LayoutUnit* inline_size) const {
    DCHECK(!is_in_parallel_flow_);

    if (*inline_size != NGSizeIndefinite)
      return;

    if (static_cast<WritingMode>(space_.writing_mode_) ==
        WritingMode::kHorizontalTb)
      *inline_size = space_.initial_containing_block_size_.width;
    else
      *inline_size = space_.initial_containing_block_size_.height;
  }

  NGConstraintSpaceBuilder& SetAvailableSize(NGLogicalSize available_size) {
    space_.available_size_ = available_size;

    if (UNLIKELY(!is_in_parallel_flow_)) {
      space_.available_size_.Flip();
      AdjustInlineSizeIfNeeded(&space_.available_size_.inline_size);
    }

    return *this;
  }

  NGConstraintSpaceBuilder& SetPercentageResolutionSize(
      NGLogicalSize percentage_resolution_size) {
    space_.percentage_resolution_size_ = percentage_resolution_size;

    if (UNLIKELY(!is_in_parallel_flow_)) {
      space_.percentage_resolution_size_.Flip();
      AdjustInlineSizeIfNeeded(&space_.percentage_resolution_size_.inline_size);
    }

    return *this;
  }

  NGConstraintSpaceBuilder& SetReplacedPercentageResolutionSize(
      NGLogicalSize replaced_percentage_resolution_size) {
    space_.replaced_percentage_resolution_size_ =
        replaced_percentage_resolution_size;

    if (UNLIKELY(!is_in_parallel_flow_)) {
      space_.replaced_percentage_resolution_size_.Flip();
      AdjustInlineSizeIfNeeded(
          &space_.replaced_percentage_resolution_size_.inline_size);
    }

    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerBlockSize(LayoutUnit size) {
    space_.fragmentainer_block_size_ = size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerSpaceAtBfcStart(LayoutUnit space) {
    space_.fragmentainer_space_at_bfc_start_ = space;
    return *this;
  }

  NGConstraintSpaceBuilder& SetTextDirection(TextDirection direction) {
    space_.direction_ = static_cast<unsigned>(direction);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsFixedSizeInline(bool b) {
    if (LIKELY(is_in_parallel_flow_))
      SetFlag(NGConstraintSpace::kFixedSizeInline, b);
    else
      SetFlag(NGConstraintSpace::kFixedSizeBlock, b);

    return *this;
  }

  NGConstraintSpaceBuilder& SetIsFixedSizeBlock(bool b) {
    if (LIKELY(is_in_parallel_flow_))
      SetFlag(NGConstraintSpace::kFixedSizeBlock, b);
    else
      SetFlag(NGConstraintSpace::kFixedSizeInline, b);

    return *this;
  }

  NGConstraintSpaceBuilder& SetFixedSizeBlockIsDefinite(bool b) {
    if (LIKELY(is_in_parallel_flow_ || !force_orthogonal_writing_mode_root_))
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

  NGConstraintSpaceBuilder& SetFragmentationType(
      NGFragmentationType fragmentation_type) {
    space_.block_direction_fragmentation_type_ = fragmentation_type;
    return *this;
  }

  NGConstraintSpaceBuilder& SetSeparateLeadingFragmentainerMargins(bool b) {
    SetFlag(NGConstraintSpace::kSeparateLeadingFragmentainerMargins, b);
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
    if (!is_new_fc_)
      space_.adjoining_floats_ = static_cast<unsigned>(floats);

    return *this;
  }

  NGConstraintSpaceBuilder& SetMarginStrut(const NGMarginStrut& margin_strut) {
    if (!is_new_fc_)
      space_.margin_strut_ = margin_strut;

    return *this;
  }

  NGConstraintSpaceBuilder& SetBfcOffset(const NGBfcOffset& bfc_offset) {
    if (!is_new_fc_)
      space_.bfc_offset_ = bfc_offset;

    return *this;
  }
  NGConstraintSpaceBuilder& SetFloatsBfcBlockOffset(
      const base::Optional<LayoutUnit>& floats_bfc_block_offset) {
    if (!is_new_fc_)
      space_.floats_bfc_block_offset_ = floats_bfc_block_offset;

    return *this;
  }

  NGConstraintSpaceBuilder& SetClearanceOffset(LayoutUnit clearance_offset) {
    if (!is_new_fc_)
      space_.clearance_offset_ = clearance_offset;

    return *this;
  }

  NGConstraintSpaceBuilder& SetShouldForceClearance(bool b) {
    SetFlag(NGConstraintSpace::kForceClearance, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetTableCellChildLayoutPhase(
      NGTableCellChildLayoutPhase table_cell_child_layout_phase) {
    space_.table_cell_child_layout_phase_ = table_cell_child_layout_phase;
    return *this;
  }

  NGConstraintSpaceBuilder& SetExclusionSpace(
      const NGExclusionSpace& exclusion_space) {
    if (!is_new_fc_)
      space_.exclusion_space_ = exclusion_space;

    return *this;
  }

  void AddBaselineRequests(const NGBaselineRequestList requests) {
    DCHECK(baseline_requests_.IsEmpty());
    baseline_requests_.AppendVector(requests);
  }
  NGConstraintSpaceBuilder& AddBaselineRequest(
      const NGBaselineRequest request) {
    baseline_requests_.push_back(request);
    return *this;
  }

  // Creates a new constraint space.
  const NGConstraintSpace ToConstraintSpace() {
#if DCHECK_IS_ON()
    DCHECK(!to_constraint_space_called_)
        << "ToConstraintSpace should only be called once.";
    to_constraint_space_called_ = true;
#endif

    DCHECK(!is_new_fc_ || !space_.adjoining_floats_);
    DCHECK_EQ(space_.HasFlag(NGConstraintSpace::kOrthogonalWritingModeRoot),
              !is_in_parallel_flow_ || force_orthogonal_writing_mode_root_);

    DCHECK(!force_orthogonal_writing_mode_root_ || is_in_parallel_flow_)
        << "Forced and inferred orthogonal writing mode shouldn't happen "
           "simultaneously. Inferred means the constraints are in parent "
           "writing mode, forced means they are in child writing mode.";

    space_.baseline_requests_ = baseline_requests_.Serialize();
    return std::move(space_);
  }

 private:
  void SetFlag(NGConstraintSpace::ConstraintSpaceFlags mask, bool value) {
    space_.flags_ = (space_.flags_ & ~static_cast<unsigned>(mask)) |
                    (-(int32_t)value & static_cast<unsigned>(mask));
  }

  NGConstraintSpace space_;
  bool is_in_parallel_flow_;
  bool is_new_fc_;
  bool force_orthogonal_writing_mode_root_;

#if DCHECK_IS_ON()
  bool to_constraint_space_called_ = false;
#endif

  NGBaselineRequestList baseline_requests_;
};

}  // namespace blink

#endif  // NGConstraintSpaceBuilder
