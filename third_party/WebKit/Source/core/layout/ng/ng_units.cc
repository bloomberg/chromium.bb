// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_units.h"

#include "core/layout/ng/ng_writing_mode.h"

namespace blink {

NGPhysicalSize NGLogicalSize::ConvertToPhysical(NGWritingMode mode) const {
  return mode == HorizontalTopBottom ? NGPhysicalSize(inline_size, block_size)
                                     : NGPhysicalSize(block_size, inline_size);
}

NGLogicalSize NGPhysicalSize::ConvertToLogical(NGWritingMode mode) const {
  return mode == HorizontalTopBottom ? NGLogicalSize(width, height)
                                     : NGLogicalSize(height, width);
}

NGPhysicalOffset NGLogicalOffset::ConvertToPhysical(
    NGWritingMode mode,
    NGDirection direction,
    NGPhysicalSize container_size,
    NGPhysicalSize inner_size) const {
  switch (mode) {
    case HorizontalTopBottom:
      if (direction == LeftToRight)
        return NGPhysicalOffset(inline_offset, block_offset);
      else
        return NGPhysicalOffset(
            container_size.width - inline_offset - inner_size.width,
            block_offset);
    case VerticalRightLeft:
    case SidewaysRightLeft:
      if (direction == LeftToRight)
        return NGPhysicalOffset(
            container_size.width - block_offset - inner_size.width,
            inline_offset);
      else
        return NGPhysicalOffset(
            container_size.width - block_offset - inner_size.width,
            container_size.height - inline_offset - inner_size.height);
    case VerticalLeftRight:
      if (direction == LeftToRight)
        return NGPhysicalOffset(block_offset, inline_offset);
      else
        return NGPhysicalOffset(
            block_offset,
            container_size.height - inline_offset - inner_size.height);
    case SidewaysLeftRight:
      if (direction == LeftToRight)
        return NGPhysicalOffset(
            block_offset,
            container_size.height - inline_offset - inner_size.height);
      else
        return NGPhysicalOffset(block_offset, inline_offset);
    default:
      ASSERT_NOT_REACHED();
      return NGPhysicalOffset();
  }
}

void NGMarginStrut::AppendMarginBlockStart(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_start =
        -std::max(value.abs(), negative_margin_block_start.abs());
  } else {
    margin_block_start = std::max(value, margin_block_start);
  }
}

void NGMarginStrut::AppendMarginBlockEnd(const LayoutUnit& value) {
  if (value < 0) {
    negative_margin_block_end =
        -std::max(value.abs(), negative_margin_block_end.abs());
  } else {
    margin_block_end = std::max(value, margin_block_end);
  }
}

String NGMarginStrut::ToString() const {
  return String::format(
      "Start: (%d %d) End: (%d %d)", negative_margin_block_start.toInt(),
      margin_block_start.toInt(), negative_margin_block_end.toInt(),
      margin_block_end.toInt());
}

}  // namespace blink
