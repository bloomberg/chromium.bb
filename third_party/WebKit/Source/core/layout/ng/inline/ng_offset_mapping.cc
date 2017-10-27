// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping.h"

#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/editing/Position.h"
#include "core/layout/ng/inline/ng_inline_node.h"

namespace blink {

namespace {

Position CreatePositionForOffsetMapping(const Node& node, unsigned dom_offset) {
  if (node.IsTextNode())
    return Position(&node, dom_offset);
  // For non-text-anchored position, the offset must be either 0 or 1.
  DCHECK_LE(dom_offset, 1u);
  return dom_offset ? Position::AfterNode(node) : Position::BeforeNode(node);
}

std::pair<const Node&, unsigned> ToNodeOffsetPair(const Position& position) {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  if (position.AnchorNode()->IsTextNode())
    return {*position.AnchorNode(), position.OffsetInContainerNode()};
  if (position.IsBeforeAnchor())
    return {*position.AnchorNode(), 0};
  return {*position.AnchorNode(), 1};
}

}  // namespace

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

unsigned NGOffsetMappingUnit::ConvertTextContentToFirstDOMOffset(
    unsigned offset) const {
  DCHECK_GE(offset, text_content_start_);
  DCHECK_LE(offset, text_content_end_);
  // Always return DOM start for collapsed units.
  if (text_content_start_ == text_content_end_)
    return dom_start_;
  // Handle identity mapping.
  if (type_ == NGOffsetMappingUnitType::kIdentity)
    return dom_start_ + offset - text_content_start_;
  // Handle expanded mapping.
  return offset < text_content_end_ ? dom_start_ : dom_end_;
}

unsigned NGOffsetMappingUnit::ConvertTextContentToLastDOMOffset(
    unsigned offset) const {
  DCHECK_GE(offset, text_content_start_);
  DCHECK_LE(offset, text_content_end_);
  // Always return DOM end for collapsed units.
  if (text_content_start_ == text_content_end_)
    return dom_end_;
  // In a non-collapsed unit, mapping between DOM and text content offsets is
  // one-to-one. Reuse existing code.
  return ConvertTextContentToFirstDOMOffset(offset);
}

// static
bool NGOffsetMapping::AcceptsPosition(const Position& position) {
  if (position.IsNull())
    return false;
  if (position.AnchorNode()->IsTextNode()) {
    return position.IsOffsetInAnchor();
  }
  if (!position.IsBeforeAnchor() && !position.IsAfterAnchor())
    return false;
  if (IsHTMLBRElement(position.AnchorNode()))
    return true;
  const LayoutObject* layout_object = position.AnchorNode()->GetLayoutObject();
  // TODO(crbug.com/776843): Support non-atomic inlines.
  return layout_object && layout_object->IsAtomicInlineLevel();
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(const Position& position) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!NGOffsetMapping::AcceptsPosition(position))
    return nullptr;
  const auto node_offset_pair = ToNodeOffsetPair(position);
  const LayoutObject* layout_object =
      AssociatedLayoutObjectOf(node_offset_pair.first, node_offset_pair.second);
  return GetFor(layout_object);
}

// static
const NGOffsetMapping* NGOffsetMapping::GetFor(
    const LayoutObject* layout_object) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return nullptr;
  if (!layout_object || !layout_object->IsInline())
    return nullptr;
  LayoutBlockFlow* block_flow = layout_object->EnclosingNGBlockFlow();
  if (!block_flow)
    return nullptr;
  DCHECK(block_flow->ChildrenInline());
  // Note: We are assuming that every LayoutNGBlockFlow with inline children is
  // laid out with NG, and hence, has offset mapping. If any change breaks this
  // assumption (e.g., disabling NG on contenteditable), extra checking is
  // needed here.
  return &NGInlineNode(block_flow).ComputeOffsetMappingIfNeeded();
}

NGOffsetMapping::NGOffsetMapping(NGOffsetMapping&& other)
    : NGOffsetMapping(std::move(other.units_),
                      std::move(other.ranges_),
                      other.text_) {}

NGOffsetMapping::NGOffsetMapping(UnitVector&& units,
                                 RangeMap&& ranges,
                                 String text)
    : units_(units), ranges_(ranges), text_(text) {}

NGOffsetMapping::~NGOffsetMapping() = default;

const NGOffsetMappingUnit* NGOffsetMapping::GetMappingUnitForDOMOffset(
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

NGMappingUnitRange NGOffsetMapping::GetMappingUnitsForDOMOffsetRange(
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

Optional<unsigned> NGOffsetMapping::GetTextContentOffset(
    const Position& position) const {
  DCHECK(NGOffsetMapping::AcceptsPosition(position)) << position;
  const auto node_offset_pair = ToNodeOffsetPair(position);
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(
      node_offset_pair.first, node_offset_pair.second);
  if (!unit)
    return WTF::nullopt;
  return unit->ConvertDOMOffsetToTextContent(node_offset_pair.second);
}

Optional<unsigned> NGOffsetMapping::StartOfNextNonCollapsedCharacter(
    const Node& node,
    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  if (!unit)
    return WTF::nullopt;

  while (unit != units_.end() && unit->GetOwner() == node) {
    if (unit->DOMEnd() > offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed)
      return std::max(offset, unit->DOMStart());
    ++unit;
  }
  return WTF::nullopt;
}

Optional<unsigned> NGOffsetMapping::EndOfLastNonCollapsedCharacter(
    const Node& node,
    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  if (!unit)
    return WTF::nullopt;

  while (unit->GetOwner() == node) {
    if (unit->DOMStart() < offset &&
        unit->GetType() != NGOffsetMappingUnitType::kCollapsed)
      return std::min(offset, unit->DOMEnd());
    if (unit == units_.begin())
      break;
    --unit;
  }
  return WTF::nullopt;
}

bool NGOffsetMapping::IsBeforeNonCollapsedCharacter(const Node& node,
                                                    unsigned offset) const {
  const NGOffsetMappingUnit* unit = GetMappingUnitForDOMOffset(node, offset);
  return unit && offset < unit->DOMEnd() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

bool NGOffsetMapping::IsAfterNonCollapsedCharacter(const Node& node,
                                                   unsigned offset) const {
  if (!offset)
    return false;
  // In case we have one unit ending at |offset| and another starting at
  // |offset|, we need to find the former. Hence, search with |offset - 1|.
  const NGOffsetMappingUnit* unit =
      GetMappingUnitForDOMOffset(node, offset - 1);
  return unit && offset > unit->DOMStart() &&
         unit->GetType() != NGOffsetMappingUnitType::kCollapsed;
}

Optional<UChar> NGOffsetMapping::GetCharacterBefore(const Node& node,
                                                    unsigned offset) const {
  Optional<unsigned> text_content_offset =
      GetTextContentOffset(CreatePositionForOffsetMapping(node, offset));
  if (!text_content_offset || !*text_content_offset)
    return WTF::nullopt;
  return text_[*text_content_offset - 1];
}

Position NGOffsetMapping::GetFirstPosition(unsigned offset) const {
  // Find the first unit where |unit.TextContentEnd() >= offset|
  if (units_.IsEmpty() || units_.back().TextContentEnd() < offset)
    return {};
  const NGOffsetMappingUnit* result =
      std::lower_bound(units_.begin(), units_.end(), offset,
                       [](const NGOffsetMappingUnit& unit, unsigned offset) {
                         return unit.TextContentEnd() < offset;
                       });
  DCHECK_NE(result, units_.end());
  if (result->TextContentStart() > offset)
    return {};
  const Node& node = result->GetOwner();
  const unsigned dom_offset =
      result->ConvertTextContentToFirstDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

Position NGOffsetMapping::GetLastPosition(unsigned offset) const {
  // Find the last unit where |unit.TextContentStart() <= offset|
  if (units_.IsEmpty() || units_.front().TextContentStart() > offset)
    return {};
  const NGOffsetMappingUnit* result =
      std::upper_bound(units_.begin(), units_.end(), offset,
                       [](unsigned offset, const NGOffsetMappingUnit& unit) {
                         return offset < unit.TextContentStart();
                       });
  DCHECK_NE(result, units_.begin());
  result = std::prev(result);
  if (result->TextContentEnd() < offset)
    return {};
  const Node& node = result->GetOwner();
  const unsigned dom_offset = result->ConvertTextContentToLastDOMOffset(offset);
  return CreatePositionForOffsetMapping(node, dom_offset);
}

}  // namespace blink
