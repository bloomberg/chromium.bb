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
    scoped_refptr<const NGPhysicalFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : NGLayoutResult(builder, /* cache_space */ true) {
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
    scoped_refptr<const NGPhysicalFragment> physical_fragment,
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
  status_ = status;
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

// We can't use =default here because RefCounted can't be copied.
NGLayoutResult::NGLayoutResult(const NGLayoutResult& other,
                               base::Optional<LayoutUnit> bfc_block_offset)
    : space_(other.space_),
      physical_fragment_(other.physical_fragment_),
      oof_positioned_descendants_(other.oof_positioned_descendants_),
      unpositioned_list_marker_(other.unpositioned_list_marker_),
      exclusion_space_(other.exclusion_space_),
      bfc_line_offset_(other.bfc_line_offset_),
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
      has_orthogonal_flow_roots_(other.has_orthogonal_flow_roots_),
      depends_on_percentage_block_size_(
          other.depends_on_percentage_block_size_),
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
      has_orthogonal_flow_roots_(builder->has_orthogonal_flow_roots_),
      depends_on_percentage_block_size_(DependsOnPercentageBlockSize(*builder)),
      status_(kSuccess) {}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLayoutResult::~NGLayoutResult() = default;

bool NGLayoutResult::DependsOnPercentageBlockSize(
    const NGContainerFragmentBuilder& builder) {
  NGLayoutInputNode node = builder.node_;

  if (!node || node.IsInline())
    return builder.has_child_that_depends_on_percentage_block_size_;

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

  if ((builder.has_child_that_depends_on_percentage_block_size_ ||
       builder.is_old_layout_root_) &&
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

}  // namespace blink
