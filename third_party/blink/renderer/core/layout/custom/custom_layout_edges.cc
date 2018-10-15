// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_edges.h"

#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"

namespace blink {

CustomLayoutEdges* CustomLayoutEdges::Create(
    const LayoutCustom& layout_custom) {
  PhysicalToLogical<LayoutUnit> logical_scrollbar(
      layout_custom.StyleRef().GetWritingMode(),
      layout_custom.StyleRef().Direction(),
      /* top */ LayoutUnit(), layout_custom.RightScrollbarWidth(),
      layout_custom.BottomScrollbarHeight(),
      layout_custom.LeftScrollbarWidth());

  return new CustomLayoutEdges(
      layout_custom.BorderAndPaddingStart() + logical_scrollbar.InlineStart(),
      layout_custom.BorderAndPaddingEnd() + logical_scrollbar.InlineEnd(),
      layout_custom.BorderAndPaddingBefore() + logical_scrollbar.BlockStart(),
      layout_custom.BorderAndPaddingAfter() + logical_scrollbar.BlockEnd());
}

CustomLayoutEdges::CustomLayoutEdges(LayoutUnit inline_start,
                                     LayoutUnit inline_end,
                                     LayoutUnit block_start,
                                     LayoutUnit block_end)
    : inline_start_(inline_start),
      inline_end_(inline_end),
      block_start_(block_start),
      block_end_(block_end) {}

CustomLayoutEdges::~CustomLayoutEdges() = default;

}  // namespace blink
