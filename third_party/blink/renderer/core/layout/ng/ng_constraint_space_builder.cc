// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"

namespace blink {

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(
    const NGConstraintSpace& parent_space)
    : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                               parent_space.InitialContainingBlockSize()) {
  parent_percentage_resolution_size_ = parent_space.PercentageResolutionSize();

  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
  if (parent_space.IsIntermediateLayout())
    flags_ |= NGConstraintSpace::kIntermediateLayout;
}

NGConstraintSpaceBuilder::NGConstraintSpaceBuilder(WritingMode writing_mode,
                                                   NGPhysicalSize icb_size)
    : initial_containing_block_size_(icb_size),
      parent_writing_mode_(writing_mode) {
  flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetAvailableSize(
    NGLogicalSize available_size) {
  available_size_ = available_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetPercentageResolutionSize(
    NGLogicalSize percentage_resolution_size) {
  percentage_resolution_size_ = percentage_resolution_size;
  return *this;
}

NGConstraintSpaceBuilder&
NGConstraintSpaceBuilder::SetReplacedPercentageResolutionSize(
    NGLogicalSize replaced_percentage_resolution_size) {
  replaced_percentage_resolution_size_ = replaced_percentage_resolution_size;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetTextDirection(
    TextDirection text_direction) {
  text_direction_ = text_direction;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetMarginStrut(
    const NGMarginStrut& margin_strut) {
  margin_strut_ = margin_strut;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetBfcOffset(
    const NGBfcOffset& bfc_offset) {
  bfc_offset_ = bfc_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFloatsBfcBlockOffset(
    const base::Optional<LayoutUnit>& floats_bfc_block_offset) {
  floats_bfc_block_offset_ = floats_bfc_block_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetClearanceOffset(
    LayoutUnit clearance_offset) {
  clearance_offset_ = clearance_offset;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetExclusionSpace(
    const NGExclusionSpace& exclusion_space) {
  exclusion_space_ = &exclusion_space;
  return *this;
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::SetFragmentationType(
    NGFragmentationType fragmentation_type) {
  fragmentation_type_ = fragmentation_type;
  return *this;
}

void NGConstraintSpaceBuilder::AddBaselineRequests(
    const Vector<NGBaselineRequest>& requests) {
  DCHECK(baseline_requests_.IsEmpty());
  baseline_requests_.AppendVector(requests);
}

NGConstraintSpaceBuilder& NGConstraintSpaceBuilder::AddBaselineRequest(
    const NGBaselineRequest& request) {
  for (const auto& existing : baseline_requests_) {
    if (existing == request)
      return *this;
  }
  baseline_requests_.push_back(request);
  return *this;
}

const NGConstraintSpace NGConstraintSpaceBuilder::ToConstraintSpace(
    WritingMode out_writing_mode) {
  return NGConstraintSpace(out_writing_mode,
                           flags_ & NGConstraintSpace::kNewFormattingContext,
                           *this);
}

}  // namespace blink
