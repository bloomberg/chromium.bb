// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping_builder.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"

namespace blink {

namespace {

// Returns the associated non-pseudo node from |layout_object|, handling
// first-letter text fragments.
const Node* GetAssociatedNode(const LayoutObject* layout_object) {
  if (!layout_object)
    return nullptr;
  if (const auto* text_fragment = ToLayoutTextFragmentOrNull(layout_object))
    return text_fragment->AssociatedTextNode();
  return layout_object->GetNode();
}

// Returns 0 unless |layout_object| is the remaining text of a node styled with
// ::first-letter, in which case it returns the start offset of the remaining
// text.
unsigned GetAssociatedStartOffset(const LayoutObject* layout_object) {
  if (const auto* text_fragment = ToLayoutTextFragmentOrNull(layout_object))
    return text_fragment->Start();
  return 0;
}

}  // namespace

NGOffsetMappingBuilder::NGOffsetMappingBuilder() = default;

NGOffsetMappingBuilder::SourceNodeScope::SourceNodeScope(
    NGOffsetMappingBuilder* builder,
    const LayoutObject* node)
    : builder_(builder),
      layout_object_auto_reset_(&builder->current_node_,
                                GetAssociatedNode(node)),
      appended_length_auto_reset_(&builder->current_offset_,
                                  GetAssociatedStartOffset(node)) {
  builder_->has_open_unit_ = false;
#if DCHECK_IS_ON()
  if (!builder_->current_node_)
    return;
  // We allow at most one scope with non-null node at any time.
  DCHECK(!builder->has_nonnull_node_scope_);
  builder->has_nonnull_node_scope_ = true;
#endif
}

NGOffsetMappingBuilder::SourceNodeScope::~SourceNodeScope() {
  builder_->has_open_unit_ = false;
#if DCHECK_IS_ON()
  if (builder_->current_node_)
    builder_->has_nonnull_node_scope_ = false;
#endif
}

void NGOffsetMappingBuilder::ReserveCapacity(unsigned capacity) {
  unit_ranges_.ReserveCapacityForSize(capacity);
  mapping_units_.ReserveCapacity(capacity * 1.5);
}

void NGOffsetMappingBuilder::AppendIdentityMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  const unsigned dom_start = current_offset_;
  const unsigned dom_end = dom_start + length;
  const unsigned text_content_start = destination_length_;
  const unsigned text_content_end = text_content_start + length;
  current_offset_ += length;
  destination_length_ += length;

  if (!current_node_)
    return;

  if (has_open_unit_ &&
      mapping_units_.back().GetType() == NGOffsetMappingUnitType::kIdentity) {
    DCHECK_EQ(mapping_units_.back().GetOwner(), current_node_);
    DCHECK_EQ(mapping_units_.back().DOMEnd(), dom_start);
    mapping_units_.back().dom_end_ += length;
    mapping_units_.back().text_content_end_ += length;
    return;
  }

  mapping_units_.emplace_back(NGOffsetMappingUnitType::kIdentity,
                              *current_node_, dom_start, dom_end,
                              text_content_start, text_content_end);
  has_open_unit_ = true;
}

void NGOffsetMappingBuilder::AppendCollapsedMapping(unsigned length) {
  DCHECK_GT(length, 0u);
  const unsigned dom_start = current_offset_;
  const unsigned dom_end = dom_start + length;
  const unsigned text_content_start = destination_length_;
  const unsigned text_content_end = text_content_start;
  current_offset_ += length;

  if (!current_node_)
    return;

  if (has_open_unit_ &&
      mapping_units_.back().GetType() == NGOffsetMappingUnitType::kCollapsed) {
    DCHECK_EQ(mapping_units_.back().GetOwner(), current_node_);
    DCHECK_EQ(mapping_units_.back().DOMEnd(), dom_start);
    mapping_units_.back().dom_end_ += length;
    return;
  }

  mapping_units_.emplace_back(NGOffsetMappingUnitType::kCollapsed,
                              *current_node_, dom_start, dom_end,
                              text_content_start, text_content_end);
  has_open_unit_ = true;
}

void NGOffsetMappingBuilder::CollapseTrailingSpace(unsigned space_offset) {
  DCHECK_LT(space_offset, destination_length_);
  --destination_length_;

  NGOffsetMappingUnit* container_unit = nullptr;
  for (unsigned i = mapping_units_.size(); i;) {
    NGOffsetMappingUnit& unit = mapping_units_[--i];
    if (unit.TextContentStart() > space_offset) {
      --unit.text_content_start_;
      --unit.text_content_end_;
      continue;
    }
    container_unit = &unit;
    break;
  }

  if (!container_unit || container_unit->TextContentEnd() <= space_offset)
    return;

  // container_unit->TextContentStart()
  // <= space_offset <
  // container_unit->TextContentEnd()
  DCHECK_EQ(NGOffsetMappingUnitType::kIdentity, container_unit->GetType());
  const Node& node = container_unit->GetOwner();
  unsigned dom_offset = container_unit->DOMStart();
  unsigned text_content_offset = container_unit->TextContentStart();
  unsigned offset_to_collapse = space_offset - text_content_offset;

  Vector<NGOffsetMappingUnit, 3> new_units;
  if (offset_to_collapse) {
    new_units.emplace_back(NGOffsetMappingUnitType::kIdentity, node, dom_offset,
                           dom_offset + offset_to_collapse, text_content_offset,
                           text_content_offset + offset_to_collapse);
    dom_offset += offset_to_collapse;
    text_content_offset += offset_to_collapse;
  }
  new_units.emplace_back(NGOffsetMappingUnitType::kCollapsed, node, dom_offset,
                         dom_offset + 1, text_content_offset,
                         text_content_offset);
  ++dom_offset;
  if (dom_offset < container_unit->DOMEnd()) {
    new_units.emplace_back(NGOffsetMappingUnitType::kIdentity, node, dom_offset,
                           container_unit->DOMEnd(), text_content_offset,
                           container_unit->TextContentEnd() - 1);
  }

  // TODO(xiaochengh): Optimize if this becomes performance bottleneck.
  unsigned position = std::distance(mapping_units_.begin(), container_unit);
  mapping_units_.EraseAt(position);
  mapping_units_.InsertVector(position, new_units);
  ShiftRanges(position, new_units.size() - 1);
  unsigned new_unit_end = position + new_units.size();
  while (new_unit_end && new_unit_end < mapping_units_.size() &&
         mapping_units_[new_unit_end - 1].Concatenate(
             mapping_units_[new_unit_end])) {
    mapping_units_.EraseAt(new_unit_end);
    ShiftRanges(new_unit_end, -1);
  }
  while (position && position < mapping_units_.size() &&
         mapping_units_[position - 1].Concatenate(mapping_units_[position])) {
    mapping_units_.EraseAt(position);
    ShiftRanges(position, -1);
  }
}

void NGOffsetMappingBuilder::ShiftRanges(unsigned position, int delta) {
  for (auto& range : unit_ranges_) {
    auto& pair = range.value;
    if (pair.first > position)
      pair.first += delta;
    if (pair.second > position)
      pair.second += delta;
  }
}

void NGOffsetMappingBuilder::SetDestinationString(String string) {
  DCHECK_EQ(destination_length_, string.length());
  destination_string_ = string;
}

NGOffsetMapping NGOffsetMappingBuilder::Build() {
  // All mapping units are already built. Scan them to build mapping ranges.
  for (unsigned range_start = 0; range_start < mapping_units_.size();) {
    const Node* node = &mapping_units_[range_start].GetOwner();
    // Units of the same node should be consecutive in the mapping function,
    // If not, the layout structure should be already broken.
    DCHECK(!unit_ranges_.Contains(node)) << node;
    unsigned range_end = range_start + 1;
    while (range_end < mapping_units_.size() &&
           mapping_units_[range_end].GetOwner() == node)
      ++range_end;
    unit_ranges_.insert(node, std::make_pair(range_start, range_end));
    range_start = range_end;
  }

  return NGOffsetMapping(std::move(mapping_units_), std::move(unit_ranges_),
                         destination_string_);
}

void NGOffsetMappingBuilder::EnterInline(const LayoutObject& layout_object) {
  if (!layout_object.NonPseudoNode())
    return;
  open_inlines_.push_back(mapping_units_.size());
}

void NGOffsetMappingBuilder::ExitInline(const LayoutObject& layout_object) {
  if (!layout_object.NonPseudoNode())
    return;
  DCHECK(open_inlines_.size());
  unit_ranges_.insert(
      layout_object.GetNode(),
      std::make_pair(open_inlines_.back(), mapping_units_.size()));
  open_inlines_.pop_back();
}

}  // namespace blink
