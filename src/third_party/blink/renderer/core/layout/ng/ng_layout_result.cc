// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalFragment> physical_fragment,
    Vector<NGOutOfFlowPositionedDescendant>&& oof_positioned_descendants,
    Vector<NGPositionedFloat>&& positioned_floats,
    const NGUnpositionedListMarker& unpositioned_list_marker,
    NGExclusionSpace&& exclusion_space,
    LayoutUnit bfc_line_offset,
    const base::Optional<LayoutUnit> bfc_block_offset,
    const NGMarginStrut end_margin_strut,
    const LayoutUnit intrinsic_block_size,
    LayoutUnit minimal_space_shortage,
    EBreakBetween initial_break_before,
    EBreakBetween final_break_after,
    bool has_forced_break,
    bool is_pushed_by_floats,
    NGFloatTypes adjoining_floats,
    NGLayoutResultStatus status)
    : unpositioned_list_marker_(unpositioned_list_marker),
      exclusion_space_(std::move(exclusion_space)),
      bfc_line_offset_(bfc_line_offset),
      bfc_block_offset_(bfc_block_offset),
      end_margin_strut_(end_margin_strut),
      intrinsic_block_size_(intrinsic_block_size),
      minimal_space_shortage_(minimal_space_shortage),
      initial_break_before_(initial_break_before),
      final_break_after_(final_break_after),
      has_forced_break_(has_forced_break),
      is_pushed_by_floats_(is_pushed_by_floats),
      adjoining_floats_(adjoining_floats),
      status_(status) {
  root_fragment_.fragment_ = std::move(physical_fragment);
  oof_positioned_descendants_.swap(oof_positioned_descendants);
  positioned_floats_.swap(positioned_floats);
}

// Define the destructor here, so that we can forward-declare more in the
// header.
NGLayoutResult::~NGLayoutResult() = default;

scoped_refptr<NGLayoutResult> NGLayoutResult::CloneWithoutOffset() const {
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants(
      oof_positioned_descendants_);
  Vector<NGPositionedFloat> positioned_floats(positioned_floats_);
  return base::AdoptRef(new NGLayoutResult(
      root_fragment_.fragment_, std::move(oof_positioned_descendants),
      std::move(positioned_floats), unpositioned_list_marker_,
      NGExclusionSpace(exclusion_space_), bfc_line_offset_, bfc_block_offset_,
      end_margin_strut_, intrinsic_block_size_, minimal_space_shortage_,
      initial_break_before_, final_break_after_, has_forced_break_,
      is_pushed_by_floats_, adjoining_floats_, Status()));
}

}  // namespace blink
