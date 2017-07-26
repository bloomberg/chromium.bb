// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_offset_mapping_result.h"

namespace blink {

NGOffsetMappingUnit::NGOffsetMappingUnit(NGOffsetMappingUnitType type,
                                         const LayoutText* owner,
                                         unsigned dom_start,
                                         unsigned dom_end,
                                         unsigned text_content_start,
                                         unsigned text_content_end)
    : type_(type),
      owner_(owner),
      dom_start_(dom_start),
      dom_end_(dom_end),
      text_content_start_(text_content_start),
      text_content_end_(text_content_end) {}

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

NGOffsetMappingResult::NGOffsetMappingResult(NGOffsetMappingResult&& other)
    : NGOffsetMappingResult(std::move(other.units_), std::move(other.ranges_)) {
}

NGOffsetMappingResult::NGOffsetMappingResult(UnitVector&& units,
                                             RangeMap&& ranges)
    : units_(units), ranges_(ranges) {}

const NGOffsetMappingUnit* NGOffsetMappingResult::GetMappingUnitForDOMOffset(
    const LayoutText* layout_text,
    unsigned offset) const {
  unsigned range_start;
  unsigned range_end;
  std::tie(range_start, range_end) = ranges_.at(layout_text);
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

}  // namespace blink
