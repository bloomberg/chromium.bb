// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_result.h"

#include "core/layout/ng/exclusions/ng_exclusion_space.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    scoped_refptr<NGPhysicalFragment> physical_fragment,
    Vector<NGOutOfFlowPositionedDescendant>& oof_positioned_descendants,
    Vector<NGPositionedFloat>& positioned_floats,
    Vector<scoped_refptr<NGUnpositionedFloat>>& unpositioned_floats,
    std::unique_ptr<const NGExclusionSpace> exclusion_space,
    const WTF::Optional<NGBfcOffset> bfc_offset,
    const NGMarginStrut end_margin_strut,
    const LayoutUnit intrinsic_block_size,
    LayoutUnit minimal_space_shortage,
    EBreakBetween initial_break_before,
    EBreakBetween final_break_after,
    bool has_forced_break,
    bool is_pushed_by_floats,
    NGLayoutResultStatus status)
    : physical_fragment_(std::move(physical_fragment)),
      exclusion_space_(std::move(exclusion_space)),
      bfc_offset_(bfc_offset),
      end_margin_strut_(end_margin_strut),
      intrinsic_block_size_(intrinsic_block_size),
      minimal_space_shortage_(minimal_space_shortage),
      initial_break_before_(initial_break_before),
      final_break_after_(final_break_after),
      has_forced_break_(has_forced_break),
      is_pushed_by_floats_(is_pushed_by_floats),
      status_(status) {
  oof_positioned_descendants_.swap(oof_positioned_descendants);
  positioned_floats_.swap(positioned_floats);
  unpositioned_floats_.swap(unpositioned_floats);
}

// Keep the implementation of the destructor here, to avoid dependencies on
// NGUnpositionedFloat in the header file.
NGLayoutResult::~NGLayoutResult() = default;

scoped_refptr<NGLayoutResult> NGLayoutResult::CloneWithoutOffset() const {
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants(
      oof_positioned_descendants_);
  Vector<NGPositionedFloat> positioned_floats(positioned_floats_);
  Vector<scoped_refptr<NGUnpositionedFloat>> unpositioned_floats(
      unpositioned_floats_);
  std::unique_ptr<const NGExclusionSpace> exclusion_space(
      WTF::WrapUnique(new NGExclusionSpace(*exclusion_space_)));
  return base::AdoptRef(new NGLayoutResult(
      physical_fragment_->CloneWithoutOffset(), oof_positioned_descendants,
      positioned_floats, unpositioned_floats, std::move(exclusion_space),
      bfc_offset_, end_margin_strut_, intrinsic_block_size_,
      minimal_space_shortage_, initial_break_before_, final_break_after_,
      has_forced_break_, is_pushed_by_floats_, Status()));
}

}  // namespace blink
