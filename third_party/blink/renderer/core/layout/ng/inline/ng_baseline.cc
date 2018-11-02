// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

const unsigned NGBaselineRequest::kTypeIdCount;
const LayoutUnit NGBaselineList::kEmptyOffset;

bool NGBaselineRequest::operator==(const NGBaselineRequest& other) const {
  return algorithm_type_ == other.algorithm_type_ &&
         baseline_type_ == other.baseline_type_;
}

bool NGBaselineRequestList::operator==(
    const NGBaselineRequestList& other) const {
  return type_id_mask_ == other.type_id_mask_;
}

void NGBaselineRequestList::push_back(const NGBaselineRequest& request) {
  type_id_mask_ |= 1 << request.TypeId();
}

void NGBaselineRequestList::AppendVector(
    const NGBaselineRequestList& requests) {
  type_id_mask_ |= requests.type_id_mask_;
}

bool NGBaseline::ShouldPropagateBaselines(const NGLayoutInputNode node) {
  if (node.IsInline())
    return true;

  return ShouldPropagateBaselines(node.GetLayoutBox());
}

bool NGBaseline::ShouldPropagateBaselines(LayoutBox* layout_box) {
  // Test if this node should use its own box to synthesize the baseline.
  if (!layout_box->IsLayoutBlock() ||
      layout_box->IsFloatingOrOutOfFlowPositioned() ||
      layout_box->IsWritingModeRoot())
    return false;

  // If this node is LayoutBlock that uses old layout, this may be a subclass
  // that overrides baseline functions. Propagate baseline requests so that we
  // call virtual functions.
  if (!NGBlockNode(layout_box).CanUseNewLayout())
    return true;

  return true;
}

NGBaselineList::NGBaselineList() {
  std::fill(std::begin(offsets_), std::end(offsets_), kEmptyOffset);
}

bool NGBaselineList::IsEmpty() const {
  for (LayoutUnit offset : offsets_) {
    if (offset != kEmptyOffset)
      return false;
  }
  return true;
}

base::Optional<LayoutUnit> NGBaselineList::Offset(
    const NGBaselineRequest request) const {
  LayoutUnit offset = offsets_[request.TypeId()];
  if (offset != kEmptyOffset)
    return offset;
  return base::nullopt;
}

void NGBaselineList::emplace_back(NGBaselineRequest request,
                                  LayoutUnit offset) {
  // Round LayoutUnit::Min() because we use it for an empty value.
  DCHECK_EQ(kEmptyOffset, LayoutUnit::Min())
      << "Change the rounding if kEmptyOffset was changed";
  if (UNLIKELY(offset == LayoutUnit::Min()))
    offset = LayoutUnit::NearlyMin();
  DCHECK_NE(offset, kEmptyOffset);
  offsets_[request.TypeId()] = offset;
}

}  // namespace blink
