// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_text_fragment_builder.h"

#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"

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
                                             NGWritingMode writing_mode)
    : NGBaseFragmentBuilder(writing_mode, TextDirection::kLtr),
      inline_node_(node) {}

void NGTextFragmentBuilder::SetItem(NGInlineItemResult* item_result,
                                    LayoutUnit line_height) {
  DCHECK(item_result);
  DCHECK(item_result->item->Style());

  SetStyle(item_result->item->Style());
  size_ = {item_result->inline_size, line_height};
  end_effect_ = item_result->text_end_effect;
  shape_result_ = std::move(item_result->shape_result);
  expansion_ = item_result->expansion;
  layout_object_ = item_result->item->GetLayoutObject();
}

void NGTextFragmentBuilder::SetText(RefPtr<const ComputedStyle> style,
                                    RefPtr<const ShapeResult> shape_result,
                                    LayoutUnit inline_size,
                                    LayoutUnit line_height) {
  DCHECK(style);

  SetStyle(style);
  size_ = {inline_size, line_height};
  end_effect_ = NGTextEndEffect::kNone;
  shape_result_ = shape_result;
  expansion_ = 0;
  layout_object_ = inline_node_.GetLayoutObject();
  ;
}

void NGTextFragmentBuilder::SetAtomicInline(RefPtr<const ComputedStyle> style,
                                            LayoutUnit inline_size,
                                            LayoutUnit line_height) {
  DCHECK(style);

  SetStyle(style);
  size_ = {inline_size, line_height};
  end_effect_ = NGTextEndEffect::kNone;
  shape_result_ = nullptr;
  expansion_ = 0;
  layout_object_ = inline_node_.GetLayoutObject();
}

RefPtr<NGPhysicalTextFragment> NGTextFragmentBuilder::ToTextFragment(
    unsigned index,
    unsigned start_offset,
    unsigned end_offset) {
  RefPtr<NGPhysicalTextFragment> fragment =
      WTF::AdoptRef(new NGPhysicalTextFragment(
          layout_object_, Style(), inline_node_.Text(), index, start_offset,
          end_offset, size_.ConvertToPhysical(WritingMode()), expansion_,
          ToLineOrientation(WritingMode()), end_effect_,
          std::move(shape_result_)));
  return fragment;
}

}  // namespace blink
