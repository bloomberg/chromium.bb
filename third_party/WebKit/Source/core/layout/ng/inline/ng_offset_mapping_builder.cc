// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_builder.h"

#include "core/layout/LayoutText.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/ng/inline/ng_offset_mapping_result.h"

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

// Returns the associated node of a possibly null LayoutObject.
const Node* GetAssociatedNode(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  if (!layout_object->IsText() ||
      !ToLayoutText(layout_object)->IsTextFragment())
    return layout_object->GetNode();
  const LayoutTextFragment* fragment = ToLayoutTextFragment(layout_object);
  return fragment->AssociatedTextNode();
}

// Finds the offset mapping unit starting from index |start|.
std::pair<NGOffsetMappingUnitType, unsigned> GetMappingUnitTypeAndEnd(
    const Vector<unsigned>& mapping,
    const Vector<const LayoutObject*>& annotation,
    unsigned start) {
  DCHECK_LT(start + 1, mapping.size());
  NGOffsetMappingUnitType type =
      GetUnitLengthMappingType(mapping[start + 1] - mapping[start]);
  if (type == NGOffsetMappingUnitType::kExpanded)
    return std::make_pair(type, start + 1);

  unsigned end = start + 1;
  for (; end + 1 < mapping.size(); ++end) {
    if (GetAssociatedNode(annotation[end]) !=
        GetAssociatedNode(annotation[start]))
      break;
    NGOffsetMappingUnitType next_type =
        GetUnitLengthMappingType(mapping[end + 1] - mapping[end]);
    if (next_type != type)
      break;
  }
  return std::make_pair(type, end);
}

// If |layout_object| is the remaining text of a text node, returns the start
// offset; In all other cases, returns 0.
unsigned GetRemainingTextOffset(const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsText())
    return 0;
  return ToLayoutText(layout_object)->TextStartOffset();
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

void NGOffsetMappingBuilder::Annotate(const LayoutObject* layout_object) {
  std::fill(annotation_.begin(), annotation_.end(), layout_object);
}

void NGOffsetMappingBuilder::AnnotateRange(unsigned start,
                                           unsigned end,
                                           const LayoutObject* layout_object) {
  DCHECK_LE(start, end);
  DCHECK_LE(end, annotation_.size());
  std::fill(annotation_.begin() + start, annotation_.begin() + end,
            layout_object);
}

void NGOffsetMappingBuilder::AnnotateSuffix(unsigned length,
                                            const LayoutObject* layout_object) {
  DCHECK_LE(length, annotation_.size());
  AnnotateRange(annotation_.size() - length, annotation_.size(), layout_object);
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

void NGOffsetMappingBuilder::SetDestinationString(String string) {
  // TODO(xiaochengh): Add the check below when we stop writing back to
  // LayoutText for inline painting.
  // DCHECK_EQ(mapping_.back(), string.length());
  destination_string_ = string;
}

NGOffsetMappingResult NGOffsetMappingBuilder::Build() const {
  NGOffsetMappingResult::UnitVector units;
  NGOffsetMappingResult::RangeMap ranges;

  const Node* current_node = nullptr;
  unsigned inline_start = 0;
  unsigned unit_range_start = 0;
  unsigned remaining_text_offset = 0;
  for (unsigned start = 0; start + 1 < mapping_.size();) {
    if (GetAssociatedNode(annotation_[start]) != current_node) {
      if (current_node) {
        ranges.insert(current_node,
                      std::make_pair(unit_range_start, units.size()));
      }
      current_node = GetAssociatedNode(annotation_[start]);
      inline_start = start;
      unit_range_start = units.size();
      // We get a non-zero |remaining_text_offset| only when |current_node| is a
      // text node that has blockified ::first-letter style, and we are at the
      // remaining text of |current_node|.
      remaining_text_offset = GetRemainingTextOffset(annotation_[start]);
    }

    if (!current_node) {
      // Only extra characters are not annotated.
      DCHECK_EQ(mapping_[start] + 1, mapping_[start + 1]);
      ++start;
      continue;
    }

    auto type_and_end = GetMappingUnitTypeAndEnd(mapping_, annotation_, start);
    NGOffsetMappingUnitType type = type_and_end.first;
    unsigned end = type_and_end.second;
    unsigned dom_start = start - inline_start + remaining_text_offset;
    unsigned dom_end = end - inline_start + remaining_text_offset;
    unsigned text_content_start = mapping_[start];
    unsigned text_content_end = mapping_[end];
    units.emplace_back(type, *current_node, dom_start, dom_end,
                       text_content_start, text_content_end);

    start = end;
  }
  if (current_node) {
    ranges.insert(current_node, std::make_pair(unit_range_start, units.size()));
  }
  return NGOffsetMappingResult(std::move(units), std::move(ranges),
                               destination_string_);
}

Vector<unsigned> NGOffsetMappingBuilder::DumpOffsetMappingForTesting() const {
  return mapping_;
}

Vector<const LayoutObject*> NGOffsetMappingBuilder::DumpAnnotationForTesting()
    const {
  return annotation_;
}

}  // namespace blink
