// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_builder.h"

#include "core/layout/ng/inline/ng_offset_mapping_result.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

// Returns the type of a unit-length simple offset mapping.
NGOffsetMappingUnitType GetUnitLengthMappingType(unsigned value) {
  if (value == 0u)
    return NGOffsetMappingUnitType::kCollapsed;
  if (value == 1u)
    return NGOffsetMappingUnitType::kIdentity;
  return NGOffsetMappingUnitType::kExpanded;
}

// Finds the offset mapping unit starting from index |start|.
std::pair<NGOffsetMappingUnitType, unsigned> GetMappingUnitTypeAndEnd(
    const Vector<unsigned>& mapping,
    const Vector<const LayoutText*>& annotation,
    unsigned start) {
  DCHECK_LT(start + 1, mapping.size());
  NGOffsetMappingUnitType type =
      GetUnitLengthMappingType(mapping[start + 1] - mapping[start]);
  if (type == NGOffsetMappingUnitType::kExpanded)
    return std::make_pair(type, start + 1);

  unsigned end = start + 1;
  for (; end + 1 < mapping.size(); ++end) {
    if (annotation[end] != annotation[start])
      break;
    NGOffsetMappingUnitType next_type =
        GetUnitLengthMappingType(mapping[end + 1] - mapping[end]);
    if (next_type != type)
      break;
  }
  return std::make_pair(type, end);
}

}  // namespace

NGOffsetMappingBuilder::NGOffsetMappingBuilder() {
  mapping_.push_back(0);
}

void NGOffsetMappingBuilder::AppendIdentityMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  DCHECK(!mapping_.IsEmpty());
  for (unsigned i = 0; i < length; ++i) {
    unsigned next = mapping_.back() + 1;
    mapping_.push_back(next);
  }
  annotation_.resize(annotation_.size() + length);
}

void NGOffsetMappingBuilder::AppendCollapsedMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  DCHECK(!mapping_.IsEmpty());
  const unsigned back = mapping_.back();
  for (unsigned i = 0; i < length; ++i)
    mapping_.push_back(back);
  annotation_.resize(annotation_.size() + length);
}

void NGOffsetMappingBuilder::CollapseTrailingSpace(unsigned skip_length) {
  DCHECK(!mapping_.IsEmpty());

  // Find the |skipped_count + 1|-st last uncollapsed character. By collapsing
  // it, all mapping values beyond this position are decremented by 1.
  unsigned skipped_count = 0;
  for (unsigned i = mapping_.size() - 1; skipped_count <= skip_length; --i) {
    DCHECK_GT(i, 0u);
    if (mapping_[i] != mapping_[i - 1])
      ++skipped_count;
    --mapping_[i];
  }
}

void NGOffsetMappingBuilder::Annotate(const LayoutText* layout_object) {
  std::fill(annotation_.begin(), annotation_.end(), layout_object);
}

void NGOffsetMappingBuilder::Concatenate(const NGOffsetMappingBuilder& other) {
  DCHECK(!mapping_.IsEmpty());
  DCHECK(!other.mapping_.IsEmpty());
  const unsigned shift_amount = mapping_.back();
  for (unsigned i = 1; i < other.mapping_.size(); ++i)
    mapping_.push_back(other.mapping_[i] + shift_amount);
  annotation_.AppendVector(other.annotation_);
}

void NGOffsetMappingBuilder::Composite(const NGOffsetMappingBuilder& other) {
  DCHECK(!mapping_.IsEmpty());
  DCHECK_EQ(mapping_.back() + 1, other.mapping_.size());
  for (unsigned i = 0; i < mapping_.size(); ++i)
    mapping_[i] = other.mapping_[mapping_[i]];
}

NGOffsetMappingResult NGOffsetMappingBuilder::Build() const {
  NGOffsetMappingResult result;

  const LayoutText* current_node = nullptr;
  unsigned inline_start = 0;
  unsigned unit_range_start = 0;
  for (unsigned start = 0; start + 1 < mapping_.size();) {
    if (annotation_[start] != current_node) {
      if (current_node) {
        result.ranges.insert(current_node, std::make_pair(unit_range_start,
                                                          result.units.size()));
      }
      current_node = annotation_[start];
      inline_start = start;
      unit_range_start = result.units.size();
    }

    if (!annotation_[start]) {
      // Only extra characters are not annotated.
      DCHECK_EQ(mapping_[start] + 1, mapping_[start + 1]);
      ++start;
      continue;
    }

    auto type_and_end = GetMappingUnitTypeAndEnd(mapping_, annotation_, start);
    NGOffsetMappingUnitType type = type_and_end.first;
    unsigned end = type_and_end.second;
    unsigned dom_start = start - inline_start + current_node->TextStartOffset();
    unsigned dom_end = end - inline_start + current_node->TextStartOffset();
    unsigned text_content_start = mapping_[start];
    unsigned text_content_end = mapping_[end];
    result.units.emplace_back(type, current_node, dom_start, dom_end,
                              text_content_start, text_content_end);

    start = end;
  }
  if (current_node) {
    result.ranges.insert(current_node,
                         std::make_pair(unit_range_start, result.units.size()));
  }
  return result;
}

Vector<unsigned> NGOffsetMappingBuilder::DumpOffsetMappingForTesting() const {
  return mapping_;
}

Vector<const LayoutText*> NGOffsetMappingBuilder::DumpAnnotationForTesting()
    const {
  return annotation_;
}

}  // namespace blink
