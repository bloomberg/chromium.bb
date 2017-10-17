// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_result.h"

#include "core/layout/ng/ng_exclusion_space.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

NGLayoutResult::NGLayoutResult(
    RefPtr<NGPhysicalFragment> physical_fragment,
    Vector<NGOutOfFlowPositionedDescendant>& oof_positioned_descendants,
    Vector<NGPositionedFloat>& positioned_floats,
    Vector<RefPtr<NGUnpositionedFloat>>& unpositioned_floats,
    std::unique_ptr<const NGExclusionSpace> exclusion_space,
    const WTF::Optional<NGBfcOffset> bfc_offset,
    const NGMarginStrut end_margin_strut,
    const LayoutUnit intrinsic_block_size,
    NGLayoutResultStatus status)
    : physical_fragment_(std::move(physical_fragment)),
      exclusion_space_(std::move(exclusion_space)),
      bfc_offset_(bfc_offset),
      end_margin_strut_(end_margin_strut),
      intrinsic_block_size_(intrinsic_block_size),
      status_(status) {
  oof_positioned_descendants_.swap(oof_positioned_descendants);
  positioned_floats_.swap(positioned_floats);
  unpositioned_floats_.swap(unpositioned_floats);
}

// Keep the implementation of the destructor here, to avoid dependencies on
// NGUnpositionedFloat in the header file.
NGLayoutResult::~NGLayoutResult() {}

RefPtr<NGLayoutResult> NGLayoutResult::CloneWithoutOffset() const {
  Vector<NGOutOfFlowPositionedDescendant> oof_positioned_descendants(
      oof_positioned_descendants_);
  Vector<NGPositionedFloat> positioned_floats(positioned_floats_);
  Vector<RefPtr<NGUnpositionedFloat>> unpositioned_floats(unpositioned_floats_);
  std::unique_ptr<const NGExclusionSpace> exclusion_space(
      WTF::WrapUnique(new NGExclusionSpace(*exclusion_space_)));
  return WTF::AdoptRef(new NGLayoutResult(
      physical_fragment_->CloneWithoutOffset(), oof_positioned_descendants,
      positioned_floats, unpositioned_floats, std::move(exclusion_space),
      bfc_offset_, end_margin_strut_, intrinsic_block_size_, Status()));
}

}  // namespace blink
