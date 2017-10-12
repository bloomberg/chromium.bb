// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_result.h"

#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/layout/ng/inline/ng_inline_node.h"

namespace blink {

NGOffsetMappingUnit::NGOffsetMappingUnit(NGOffsetMappingUnitType type,
                                         const Node& node,
                                         unsigned dom_start,
                                         unsigned dom_end,
                                         unsigned text_content_start,
                                         unsigned text_content_end)
    : type_(type),
      owner_(&node),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {}

NGOffsetMappingUnit::~NGOffsetMappingUnit() = default;

unsigned NGOffsetMappingUnit::ConvertDOMOffsetToTextContent(
    unsigned offset) const {
  DCHECK_GE(offset, dom_start_);
  DCHECK_LE(offset, dom_end_);
  // DOM start is always mapped to text content start.
  if (offset == dom_start_)
    return text_content_start_;
  // DOM end is always mapped to text content end.
  if (offset == dom_end_)
    return text_content_end_;
  // Handle collapsed mapping.
  if (text_content_start_ == text_content_end_)
    return text_content_start_;
  // Handle has identity mapping.
  return offset - dom_start_ + text_content_start_;
}

const NGOffsetMappingResult* GetNGOffsetMappingFor(const Node& node,
                                                   unsigned offset) {
  Optional<NGInlineNode> inline_node = GetNGInlineNodeFor(node, offset);
  if (!inline_node)
    return nullptr;
  return &inline_node->ComputeOffsetMappingIfNeeded();
}

NGOffsetMappingResult::NGOffsetMappingResult(NGOffsetMappingResult&& other)
    : NGOffsetMappingResult(std::move(other.units_), std::move(other.ranges_)) {
}

NGOffsetMappingResult::NGOffsetMappingResult(UnitVector&& units,
                                             RangeMap&& ranges)
    : units_(units), ranges_(ranges) {}

NGOffsetMappingResult::~NGOffsetMappingResult() = default;

const NGOffsetMappingUnit* NGOffsetMappingResult::GetMappingUnitForDOMOffset(
    const Node& node,
    unsigned offset) const {
  unsigned range_start;
  unsigned range_end;
  std::tie(range_start, range_end) = ranges_.at(&node);
  if (range_start == range_end || units_[range_start].DOMStart() > offset)
    return nullptr;
  // Find the last unit where unit.dom_start <= offset
  const NGOffsetMappingUnit* unit = std::prev(std::upper_bound(
      units_.begin() + range_start, units_.begin() + range_end, offset,
      [](unsigned offset, const NGOffsetMappingUnit& unit) {
        return offset < unit.DOMStart();
      }));
  if (unit->DOMEnd() < offset)
    return nullptr;
  return unit;
}

NGMappingUnitRange NGOffsetMappingResult::GetMappingUnitsForDOMOffsetRange(
    const Node& node,
    unsigned start_offset,
    unsigned end_offset) const {
  unsigned range_start;
  unsigned range_end;
  std::tie(range_start, range_end) = ranges_.at(&node);
  if (range_start == range_end || units_[range_start].DOMStart() > end_offset ||
      units_[range_end - 1].DOMEnd() < start_offset)
    return {};

  // Find the first unit where unit.dom_end >= start_offset
  const NGOffsetMappingUnit* result_begin = std::lower_bound(
      units_.begin() + range_start, units_.begin() + range_end, start_offset,
      [](const NGOffsetMappingUnit& unit, unsigned offset) {
        return unit.DOMEnd() < offset;
      });

  // Find the next of the last unit where unit.dom_start <= end_offset
  const NGOffsetMappingUnit* result_end =
      std::upper_bound(result_begin, units_.begin() + range_end, end_offset,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset < unit.DOMStart();
                       });

  return {result_begin, result_end};
}

size_t NGOffsetMappingResult::GetTextContentOffset(const Node& node,
                                                   unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  if (!unit)
    return kNotFound;
  return unit->ConvertDOMOffsetToTextContent(offset);
}

unsigned NGOffsetMappingResult::StartOfNextNonCollapsedCharacter(
    const Node& node,
    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  if (!unit) {
    // It is possible for a fully collapsed whitespace text node to not have a
    // LayoutObject, in which case it is not in the offset mapping.
    if (node.IsTextNode())
      return ToText(node).length();
    NOTREACHED() << node << "@" << offset;
    return offset;
  }

  unsigned fallback = offset;
  while (unit != units_.end() && unit->GetOwner() == node) {
    if (unit->DOMEnd() > offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed)
      return std::max(offset, unit->DOMStart());
    fallback = unit->DOMEnd();
    ++unit;
  }
  return fallback;
}

unsigned NGOffsetMappingResult::EndOfLastNonCollapsedCharacter(
    const Node& node,
    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  if (!unit) {
    // It is possible for a fully collapsed whitespace text node to not have a
    // LayoutObject, in which case it is not in the offset mapping.
    DCHECK(node.IsTextNode()) << node << "@" << offset;
    return 0;
  }

  unsigned fallback = offset;
  while (unit->GetOwner() == node) {
    if (unit->DOMStart() < offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed)
      return std::min(offset, unit->DOMEnd());
    fallback = unit->DOMStart();
    if (unit == units_.begin())
      break;
    --unit;
  }
  return fallback;
}

bool NGOffsetMappingResult::IsNonCollapsedCharacter(const Node& node,
                                                    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  return unit && offset < unit->DOMEnd() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

}  // namespace blink
