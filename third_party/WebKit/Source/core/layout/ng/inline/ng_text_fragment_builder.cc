// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_text_fragment_builder.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "platform/heap/Handle.h"

namespace blink {

namespace {

NGLineOrientation ToLineOrientation(NGWritingMode writing_mode) {
  switch (writing_mode) {
    case NGWritingMode::kHorizontalTopBottom:
      return NGLineOrientation::kHorizontal;
    case NGWritingMode::kVerticalRightLeft:
    case NGWritingMode::kVerticalLeftRight:
    case NGWritingMode::kSidewaysRightLeft:
      return NGLineOrientation::kClockWiseVertical;
    case NGWritingMode::kSidewaysLeftRight:
      return NGLineOrientation::kCounterClockWiseVertical;
  }
  NOTREACHED();
  return NGLineOrientation::kHorizontal;
}

}  // namespace

NGTextFragmentBuilder::NGTextFragmentBuilder(NGInlineNode node,
                                             const ComputedStyle& style,
                                             NGWritingMode writing_mode)
    : NGBaseFragmentBuilder(style, writing_mode, TextDirection::kLtr),
      node_(node) {}

NGTextFragmentBuilder::NGTextFragmentBuilder(NGInlineNode node,
                                             NGWritingMode writing_mode)
    : NGBaseFragmentBuilder(writing_mode, TextDirection::kLtr), node_(node) {}

NGTextFragmentBuilder& NGTextFragmentBuilder::SetSize(
    const NGLogicalSize& size) {
  size_ = size;
  return *this;
}

NGTextFragmentBuilder& NGTextFragmentBuilder::SetShapeResult(
    RefPtr<const ShapeResult> shape_result) {
  shape_result_ = shape_result;
  return *this;
}

RefPtr<NGPhysicalTextFragment> NGTextFragmentBuilder::ToTextFragment(
    unsigned index,
    unsigned start_offset,
    unsigned end_offset) {
  return AdoptRef(new NGPhysicalTextFragment(
      node_.GetLayoutObject(), Style(), node_.Text(), index, start_offset,
      end_offset, size_.ConvertToPhysical(WritingMode()),
      ToLineOrientation(WritingMode()), std::move(shape_result_)));
}

}  // namespace blink
