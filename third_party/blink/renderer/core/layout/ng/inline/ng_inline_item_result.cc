// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult()
    : item(nullptr), item_index(0), start_offset(0), end_offset(0) {}

NGInlineItemResult::NGInlineItemResult(const NGInlineItem* item,
                                       unsigned index,
                                       unsigned start,
                                       unsigned end,
                                       bool break_anywhere_if_overflow,
                                       bool should_create_line_box,
                                       bool has_unpositioned_floats)
    : item(item),
      item_index(index),
      start_offset(start),
      end_offset(end),
      break_anywhere_if_overflow(break_anywhere_if_overflow),
      should_create_line_box(should_create_line_box),
      has_unpositioned_floats(has_unpositioned_floats) {}

void NGLineInfo::SetLineStyle(const NGInlineNode& node,
                              const NGInlineItemsData& items_data,
                              bool use_first_line_style) {
  use_first_line_style_ = use_first_line_style;
  items_data_ = &items_data;
  line_style_ = node.GetLayoutBox()->Style(use_first_line_style_);
}

#if DCHECK_IS_ON()
void NGInlineItemResult::CheckConsistency(bool allow_null_shape_result) const {
  DCHECK(item);
  if (item->Type() == NGInlineItem::kText) {
    DCHECK_LT(start_offset, end_offset);
    if (allow_null_shape_result && !shape_result)
      return;
    DCHECK(shape_result);
    DCHECK_EQ(end_offset - start_offset, shape_result->NumCharacters());
    DCHECK_EQ(start_offset, shape_result->StartIndex());
    DCHECK_EQ(end_offset, shape_result->EndIndex());
  }
}
#endif

unsigned NGLineInfo::InflowEndOffset() const {
  const NGInlineItemResults& item_results = Results();
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kControl ||
        item.Type() == NGInlineItem::kAtomicInline)
      return item_result.end_offset;
  }
  return StartOffset();
}

LayoutUnit NGLineInfo::ComputeTrailingSpaceWidth(
    unsigned* end_offset_out) const {
  if (!has_trailing_spaces_) {
    if (end_offset_out)
      *end_offset_out = InflowEndOffset();
    return LayoutUnit();
  }

  const NGInlineItemResults& item_results = Results();
  LayoutUnit trailing_spaces_width;
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;

    // If this item is opaque to whitespace collapsing, whitespace before this
    // item maybe collapsed. Keep looking for previous items.
    if (item.EndCollapseType() == NGInlineItem::kOpaqueToCollapsing) {
      continue;
    }
    // These items should be opaque-to-collapsing.
    DCHECK(item.Type() != NGInlineItem::kFloating &&
           item.Type() != NGInlineItem::kOutOfFlowPositioned &&
           item.Type() != NGInlineItem::kBidiControl);

    if (item.Type() == NGInlineItem::kControl ||
        item_result.has_only_trailing_spaces) {
      trailing_spaces_width += item_result.inline_size;
      continue;
    }

    // The last text item may contain trailing spaces if this is a last line,
    // has a forced break, or is 'white-space: pre'.
    unsigned end_offset = item_result.end_offset;
    DCHECK(end_offset);
    if (item.Type() == NGInlineItem::kText) {
      const String& text = items_data_->text_content;
      if (end_offset && text[end_offset - 1] == kSpaceCharacter) {
        do {
          --end_offset;
        } while (end_offset > item_result.start_offset &&
                 text[end_offset - 1] == kSpaceCharacter);

        // If all characters in this item_result are spaces, check next item.
        if (end_offset == item_result.start_offset) {
          trailing_spaces_width += item_result.inline_size;
          continue;
        }

        // To compute the accurate width, we need to reshape if |end_offset| is
        // not safe-to-break. We avoid reshaping in this case because the cost
        // is high and the difference is subtle for the purpose of this
        // function.
        // TODO(kojii): Compute this without |CreateShapeResult|.
        scoped_refptr<ShapeResult> shape_result =
            item_result.shape_result->CreateShapeResult();
        float end_position = shape_result->PositionForOffset(
            end_offset - shape_result->StartIndex());
        trailing_spaces_width += shape_result->Width() - end_position;
      }
    }

    if (end_offset_out)
      *end_offset_out = end_offset;
    return trailing_spaces_width;
  }

  // An empty line, or only trailing spaces.
  if (end_offset_out)
    *end_offset_out = StartOffset();
  return trailing_spaces_width;
}

LayoutUnit NGLineInfo::ComputeWidth() const {
  LayoutUnit inline_size = TextIndent();
  for (const NGInlineItemResult& item_result : Results())
    inline_size += item_result.inline_size;

  if (UNLIKELY(line_end_fragment_)) {
    inline_size += line_end_fragment_->Size()
                       .ConvertToLogical(LineStyle().GetWritingMode())
                       .inline_size;
  }

  return inline_size;
}

void NGLineInfo::SetLineEndFragment(
    scoped_refptr<const NGPhysicalTextFragment> fragment) {
  line_end_fragment_ = std::move(fragment);
}

}  // namespace blink
