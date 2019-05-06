// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : NGLayoutResult(builder, /* cache_space */ true) {
  is_initial_block_size_indefinite_ =
      builder->is_initial_block_size_indefinite_;
  intrinsic_block_size_ = builder->intrinsic_block_size_;
  minimal_space_shortage_ = builder->minimal_space_shortage_;
  initial_break_before_ = builder->initial_break_before_;
  final_break_after_ = builder->previous_break_after_;
  has_forced_break_ = builder->has_forced_break_;
  DCHECK(physical_fragment) << "Use the other constructor for aborting layout";
  physical_fragment_ = std::move(physical_fragment);
  oof_positioned_descendants_ = std::move(builder->oof_positioned_descendants_);
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGLineBoxFragmentBuilder* builder)
    : NGLayoutResult(builder, /* cache_space */ false) {
  physical_fragment_ = std::move(physical_fragment);
  oof_positioned_descendants_ = std::move(builder->oof_positioned_descendants_);
}

NGLayoutResult::NGLayoutResult(NGLayoutResultStatus status,
                               NGBoxFragmentBuilder* builder)
    : NGLayoutResult(builder, /* cache_space */ false) {
  adjoining_floats_ = kFloatTypeNone;
  depends_on_percentage_block_size_ = false;
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
      oof_positioned_descendants_(other.oof_positioned_descendants_),
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
      is_pushed_by_floats_(other.is_pushed_by_floats_),
      adjoining_floats_(other.adjoining_floats_),
      is_initial_block_size_indefinite_(
          other.is_initial_block_size_indefinite_),
      has_orthogonal_flow_roots_(other.has_orthogonal_flow_roots_),
      may_have_descendant_above_block_start_(
          other.may_have_descendant_above_block_start_),
      depends_on_percentage_block_size_(
          other.depends_on_percentage_block_size_),
      has_descendant_that_depends_on_percentage_block_size_(
          other.has_descendant_that_depends_on_percentage_block_size_),
      status_(other.status_) {}

NGLayoutResult::NGLayoutResult(NGContainerFragmentBuilder* builder,
                               bool cache_space)
    : space_(cache_space && builder->space_
                 ? NGConstraintSpace(*builder->space_)
                 : NGConstraintSpace()),
      unpositioned_list_marker_(builder->unpositioned_list_marker_),
      exclusion_space_(std::move(builder->exclusion_space_)),
      bfc_line_offset_(builder->bfc_line_offset_),
      bfc_block_offset_(builder->bfc_block_offset_),
      end_margin_strut_(builder->end_margin_strut_),
      has_valid_space_(cache_space && builder->space_),
      has_forced_break_(false),
      is_pushed_by_floats_(builder->is_pushed_by_floats_),
      adjoining_floats_(builder->adjoining_floats_),
      is_initial_block_size_indefinite_(false),
      has_orthogonal_flow_roots_(builder->has_orthogonal_flow_roots_),
      may_have_descendant_above_block_start_(
          builder->may_have_descendant_above_block_start_),
      depends_on_percentage_block_size_(DependsOnPercentageBlockSize(*builder)),
      has_descendant_that_depends_on_percentage_block_size_(
          builder->has_descendant_that_depends_on_percentage_block_size_),
      status_(kSuccess) {}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLayoutResult::~NGLayoutResult() = default;

bool NGLayoutResult::DependsOnPercentageBlockSize(
    const NGContainerFragmentBuilder& builder) {
  NGLayoutInputNode node = builder.node_;

  if (!node || node.IsInline())
    return builder.has_descendant_that_depends_on_percentage_block_size_;

  // NOTE: If an element is OOF positioned, and has top/bottom constraints
  // which are percentage based, this function will return false.
  //
  // This is fine as the top/bottom constraints are computed *before* layout,
  // and the result is set as a fixed-block-size constraint. (And the caching
  // logic will never check the result of this function).
  //
  // The result of this function still may be used for an OOF positioned
  // element if it has a percentage block-size however, but this will return
  // the correct result from below.

  if ((builder.has_descendant_that_depends_on_percentage_block_size_ ||
       builder.is_legacy_layout_root_) &&
      node.UseParentPercentageResolutionBlockSizeForChildren()) {
    // Quirks mode has different %-block-size behaviour, than standards mode.
    // An arbitrary descendant may depend on the percentage resolution
    // block-size given.
    // If this is also an anonymous block we need to mark ourselves dependent
    // if we have a dependent child.
    return true;
  }

  const ComputedStyle& style = builder.Style();
  if (style.LogicalHeight().IsPercentOrCalc() ||
      style.LogicalMinHeight().IsPercentOrCalc() ||
      style.LogicalMaxHeight().IsPercentOrCalc())
    return true;

  return false;
}

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

}  // namespace blink
