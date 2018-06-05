// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

bool NGBoxStrut::IsEmpty() const {
  return *this == NGBoxStrut();
}

bool NGBoxStrut::operator==(const NGBoxStrut& other) const {
  return std::tie(other.inline_start, other.inline_end, other.block_start,
                  other.block_end) ==
         std::tie(inline_start, inline_end, block_start, block_end);
}

NGPhysicalBoxStrut NGBoxStrut::ConvertToPhysical(
    WritingMode writing_mode,
    TextDirection direction) const {
  LayoutUnit direction_start = inline_start;
  LayoutUnit direction_end = inline_end;
  if (direction == TextDirection::kRtl)
    std::swap(direction_start, direction_end);
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return NGPhysicalBoxStrut(block_start, direction_end, block_end,
                                direction_start);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return NGPhysicalBoxStrut(direction_start, block_start, direction_end,
                                block_end);
    case WritingMode::kVerticalLr:
      return NGPhysicalBoxStrut(direction_start, block_end, direction_end,
                                block_start);
    case WritingMode::kSidewaysLr:
      return NGPhysicalBoxStrut(direction_end, block_end, direction_start,
                                block_start);
    default:
      NOTREACHED();
      return NGPhysicalBoxStrut();
  }
}

// Converts physical dimensions to logical ones per
// https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
NGBoxStrut NGPhysicalBoxStrut::ConvertToLogical(WritingMode writing_mode,
                                                TextDirection direction) const {
  NGBoxStrut strut;
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      strut = {left, right, top, bottom};
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      strut = {top, bottom, right, left};
      break;
    case WritingMode::kVerticalLr:
      strut = {top, bottom, left, right};
      break;
    case WritingMode::kSidewaysLr:
      strut = {bottom, top, left, right};
      break;
  }
  if (direction == TextDirection::kRtl)
    std::swap(strut.inline_start, strut.inline_end);
  return strut;
}

String NGBoxStrut::ToString() const {
  return String::Format("Inline: (%d %d) Block: (%d %d)", inline_start.ToInt(),
                        inline_end.ToInt(), block_start.ToInt(),
                        block_end.ToInt());
}

std::ostream& operator<<(std::ostream& stream, const NGBoxStrut& value) {
  return stream << value.ToString();
}

NGBoxStrut::NGBoxStrut(const NGLineBoxStrut& line_relative,
                       bool is_flipped_lines) {
  if (!is_flipped_lines) {
    *this = {line_relative.inline_start, line_relative.inline_end,
             line_relative.line_over, line_relative.line_under};
  } else {
    *this = {line_relative.inline_start, line_relative.inline_end,
             line_relative.line_under, line_relative.line_over};
  }
}

NGLineBoxStrut::NGLineBoxStrut(const NGBoxStrut& flow_relative,
                               bool is_flipped_lines) {
  if (!is_flipped_lines) {
    *this = {flow_relative.inline_start, flow_relative.inline_end,
             flow_relative.block_start, flow_relative.block_end};
  } else {
    *this = {flow_relative.inline_start, flow_relative.inline_end,
             flow_relative.block_end, flow_relative.block_start};
  }
}

NGPixelSnappedPhysicalBoxStrut NGPhysicalBoxStrut::SnapToDevicePixels() const {
  return NGPixelSnappedPhysicalBoxStrut(top.Round(), right.Round(),
                                        bottom.Round(), left.Round());
}

}  // namespace blink
