// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_text_fragment_builder.h"

#include "core/layout/ng/inline/ng_inline_item_result.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_height_metrics.h"
#include "core/layout/ng/inline/ng_physical_text_fragment.h"

namespace blink {

namespace {

NGLineOrientation ToLineOrientation(WritingMode writing_mode) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return NGLineOrientation::kHorizontal;
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      return NGLineOrientation::kClockWiseVertical;
    case WritingMode::kSidewaysLr:
      return NGLineOrientation::kCounterClockWiseVertical;
  }
  NOTREACHED();
  return NGLineOrientation::kHorizontal;
}

}  // namespace

NGTextFragmentBuilder::NGTextFragmentBuilder(NGInlineNode node,
                                             WritingMode writing_mode)
    : NGBaseFragmentBuilder(writing_mode, TextDirection::kLtr),
      inline_node_(node) {}

void NGTextFragmentBuilder::SetItem(NGInlineItemResult* item_result,
                                    LayoutUnit line_height) {
  DCHECK(item_result);
  DCHECK(item_result->item->Style());

  text_ = inline_node_.Text();
  item_index_ = item_result->item_index;
  start_offset_ = item_result->start_offset;
  end_offset_ = item_result->end_offset;
  SetStyle(item_result->item->Style());
  size_ = {item_result->inline_size, line_height};
  end_effect_ = item_result->text_end_effect;
  shape_result_ = std::move(item_result->shape_result);
  expansion_ = item_result->expansion;
  layout_object_ = item_result->item->GetLayoutObject();
}

void NGTextFragmentBuilder::SetText(
    LayoutObject* layout_object,
    const String& text,
    scoped_refptr<const ComputedStyle> style,
    scoped_refptr<const ShapeResult> shape_result) {
  DCHECK(layout_object);
  DCHECK(style);
  DCHECK(shape_result);

  text_ = text;
  item_index_ = std::numeric_limits<unsigned>::max();
  start_offset_ = shape_result->StartIndexForResult();
  end_offset_ = shape_result->EndIndexForResult();
  SetStyle(style);
  FontBaseline baseline_type = style->IsHorizontalWritingMode()
                                   ? kAlphabeticBaseline
                                   : kIdeographicBaseline;
  size_ = {shape_result->SnappedWidth(),
           NGLineHeightMetrics(*style, baseline_type).LineHeight()};
  shape_result_ = std::move(shape_result);
  layout_object_ = layout_object;
  end_effect_ = NGTextEndEffect::kNone;
  expansion_ = 0;
}

scoped_refptr<NGPhysicalTextFragment> NGTextFragmentBuilder::ToTextFragment() {
  scoped_refptr<NGPhysicalTextFragment> fragment =
      base::AdoptRef(new NGPhysicalTextFragment(
          layout_object_, Style(), text_, item_index_, start_offset_,
          end_offset_, size_.ConvertToPhysical(GetWritingMode()), expansion_,
          ToLineOrientation(GetWritingMode()), end_effect_,
          std::move(shape_result_)));
  return fragment;
}

}  // namespace blink
