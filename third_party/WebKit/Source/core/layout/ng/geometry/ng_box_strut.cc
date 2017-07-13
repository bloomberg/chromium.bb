// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_box_strut.h"

#include "platform/wtf/text/WTFString.h"

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
    NGWritingMode writing_mode,
    TextDirection direction) const {
  LayoutUnit direction_start = inline_start;
  LayoutUnit direction_end = inline_end;
  if (direction == TextDirection::kRtl)
    std::swap(direction_start, direction_end);
  switch (writing_mode) {
    case kHorizontalTopBottom:
      return NGPhysicalBoxStrut(block_start, direction_end, block_end,
                                direction_start);
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      return NGPhysicalBoxStrut(direction_start, block_start, direction_end,
                                block_end);
    case kVerticalLeftRight:
      return NGPhysicalBoxStrut(direction_start, block_end, direction_end,
                                block_start);
    case kSidewaysLeftRight:
      return NGPhysicalBoxStrut(direction_end, block_end, direction_start,
                                block_start);
    default:
      NOTREACHED();
      return NGPhysicalBoxStrut();
  }
}

// Converts physical dimensions to logical ones per
// https://drafts.csswg.org/css-writing-modes-3/#logical-to-physical
NGBoxStrut NGPhysicalBoxStrut::ConvertToLogical(NGWritingMode writing_mode,
                                                TextDirection direction) const {
  NGBoxStrut strut;
  switch (writing_mode) {
    case kHorizontalTopBottom:
      strut = {left, right, top, bottom};
      break;
    case kVerticalRightLeft:
    case kSidewaysRightLeft:
      strut = {top, bottom, right, left};
      break;
    case kVerticalLeftRight:
      strut = {top, bottom, left, right};
      break;
    case kSidewaysLeftRight:
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

NGPixelSnappedPhysicalBoxStrut NGPhysicalBoxStrut::SnapToDevicePixels() const {
  return NGPixelSnappedPhysicalBoxStrut(top.Round(), right.Round(),
                                        bottom.Round(), left.Round());
}

}  // namespace blink
