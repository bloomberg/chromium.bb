// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     builder,
                     /* cache_space */ true) {
  is_initial_block_size_indefinite_ =
      builder->is_initial_block_size_indefinite_;
  intrinsic_block_size_ = builder->intrinsic_block_size_;
  minimal_space_shortage_ = builder->minimal_space_shortage_;
  initial_break_before_ = builder->initial_break_before_;
  final_break_after_ = builder->previous_break_after_;
  has_forced_break_ = builder->has_forced_break_;
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGLineBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     builder,
                     /* cache_space */ false) {}

NGLayoutResult::NGLayoutResult(NGLayoutResultStatus status,
                               NGBoxFragmentBuilder* builder)
    : NGLayoutResult(/* physical_fragment */ nullptr,
                     builder,
                     /* cache_space */ false) {
  adjoining_floats_ = kFloatTypeNone;
  has_descendant_that_depends_on_percentage_block_size_ = false;
  status_ = status;
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

NGLayoutResult::NGLayoutResult(const NGLayoutResult& other,
                               const NGConstraintSpace& new_space,
                               LayoutUnit bfc_line_offset,
                               base::Optional<LayoutUnit> bfc_block_offset)
    : space_(new_space),
      physical_fragment_(other.physical_fragment_),
      unpositioned_list_marker_(other.unpositioned_list_marker_),
      exclusion_space_(MergeExclusionSpaces(other,
                                            space_.ExclusionSpace(),
                                            bfc_line_offset,
                                            bfc_block_offset)),
      bfc_line_offset_(bfc_line_offset),
      bfc_block_offset_(bfc_block_offset),
      end_margin_strut_(other.end_margin_strut_),
      intrinsic_block_size_(other.intrinsic_block_size_),
      minimal_space_shortage_(other.minimal_space_shortage_),
      initial_break_before_(other.initial_break_before_),
      final_break_after_(other.final_break_after_),
      has_valid_space_(other.has_valid_space_),
      has_forced_break_(other.has_forced_break_),
      is_self_collapsing_(other.is_self_collapsing_),
      is_pushed_by_floats_(other.is_pushed_by_floats_),
      adjoining_floats_(other.adjoining_floats_),
      is_initial_block_size_indefinite_(
          other.is_initial_block_size_indefinite_),
      has_descendant_that_depends_on_percentage_block_size_(
          other.has_descendant_that_depends_on_percentage_block_size_),
      status_(other.status_) {}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGContainerFragmentBuilder* builder,
    bool cache_space)
    : space_(cache_space && builder->space_
                 ? NGConstraintSpace(*builder->space_)
                 : NGConstraintSpace()),
      physical_fragment_(std::move(physical_fragment)),
      unpositioned_list_marker_(builder->unpositioned_list_marker_),
      exclusion_space_(std::move(builder->exclusion_space_)),
      bfc_line_offset_(builder->bfc_line_offset_),
      bfc_block_offset_(builder->bfc_block_offset_),
      end_margin_strut_(builder->end_margin_strut_),
      has_valid_space_(cache_space && builder->space_),
      has_forced_break_(false),
      is_self_collapsing_(builder->is_self_collapsing_),
      is_pushed_by_floats_(builder->is_pushed_by_floats_),
      adjoining_floats_(builder->adjoining_floats_),
      is_initial_block_size_indefinite_(false),
      has_descendant_that_depends_on_percentage_block_size_(
          builder->has_descendant_that_depends_on_percentage_block_size_),
      status_(kSuccess) {
#if DCHECK_IS_ON()
  if (is_self_collapsing_ && physical_fragment_) {
    // A new formatting-context shouldn't be self-collapsing.
    DCHECK(!physical_fragment_->IsBlockFormattingContextRoot());

    // Self-collapsing children must have a block-size of zero.
    NGFragment fragment(physical_fragment_->Style().GetWritingMode(),
                        *physical_fragment_);
    DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
  }
#endif
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLayoutResult::~NGLayoutResult() = default;

NGExclusionSpace NGLayoutResult::MergeExclusionSpaces(
    const NGLayoutResult& other,
    const NGExclusionSpace& new_input_exclusion_space,
    LayoutUnit bfc_line_offset,
    base::Optional<LayoutUnit> bfc_block_offset) {
  // If we are merging exclusion spaces we should be copying a previous layout
  // result. It is impossible to reach a state where bfc_block_offset has a
  // value, and the result which we are copying doesn't (or visa versa).
  // This would imply the result has switched its "empty" state for margin
  // collapsing, which would mean it isn't possible to reuse the result.
  DCHECK_EQ(bfc_block_offset.has_value(), other.bfc_block_offset_.has_value());

  NGBfcDelta offset_delta = {bfc_line_offset - other.bfc_line_offset_,
                             bfc_block_offset && other.bfc_block_offset_
                                 ? *bfc_block_offset - *other.bfc_block_offset_
                                 : LayoutUnit()};

  return NGExclusionSpace::MergeExclusionSpaces(
      /* old_output */ other.exclusion_space_,
      /* old_input */ other.space_.ExclusionSpace(),
      /* new_input */ new_input_exclusion_space, offset_delta);
}

#if DCHECK_IS_ON()
void NGLayoutResult::CheckSameForSimplifiedLayout(
    const NGLayoutResult& other,
    bool check_same_block_size) const {
  To<NGPhysicalBoxFragment>(*physical_fragment_)
      .CheckSameForSimplifiedLayout(
          To<NGPhysicalBoxFragment>(*other.physical_fragment_),
          check_same_block_size);

  DCHECK(unpositioned_list_marker_ == other.unpositioned_list_marker_);
  exclusion_space_.CheckSameForSimplifiedLayout(other.exclusion_space_);

  // We ignore bfc_block_offset_, and bfc_line_offset_ as "simplified" layout
  // will move the layout result if required.

  // We ignore the intrinsic_block_size_ as if a scrollbar gets added/removed
  // this may change (even if the size of the fragment remains the same).

  DCHECK(end_margin_strut_ == other.end_margin_strut_);
  DCHECK_EQ(minimal_space_shortage_, other.minimal_space_shortage_);

  DCHECK_EQ(initial_break_before_, other.initial_break_before_);
  DCHECK_EQ(final_break_after_, other.final_break_after_);

  DCHECK_EQ(has_valid_space_, other.has_valid_space_);
  DCHECK_EQ(has_forced_break_, other.has_forced_break_);
  DCHECK_EQ(is_self_collapsing_, other.is_self_collapsing_);
  DCHECK_EQ(is_pushed_by_floats_, other.is_pushed_by_floats_);
  DCHECK_EQ(adjoining_floats_, other.adjoining_floats_);

  if (check_same_block_size) {
    DCHECK_EQ(is_initial_block_size_indefinite_,
              other.is_initial_block_size_indefinite_);
  }
  DCHECK_EQ(has_descendant_that_depends_on_percentage_block_size_,
            other.has_descendant_that_depends_on_percentage_block_size_);
  DCHECK_EQ(status_, other.status_);
}
#endif

}  // namespace blink
