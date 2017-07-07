// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_box_fragment.h"
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "core/layout/ng/inline/ng_line_breaker.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment_builder.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/BidiRunList.h"

namespace blink {
namespace {

inline bool ShouldCreateBoxFragment(const NGInlineItem& item,
                                    const NGInlineItemResult& item_result) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  // TODO(kojii): We might need more conditions to create box fragments.
  return style.HasBoxDecorationBackground() || item_result.needs_box_when_empty;
}

NGLogicalOffset GetOriginPointForFloats(
    const NGLogicalOffset& container_bfc_offset,
    LayoutUnit content_size) {
  NGLogicalOffset origin_point = container_bfc_offset;
  origin_point.block_offset += content_size;
  return origin_point;
}

}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode inline_node,
    NGConstraintSpace* space,
    NGInlineBreakToken* break_token)
    : NGLayoutAlgorithm(inline_node, space, break_token),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(space->WritingMode())),
      space_builder_(space) {
  container_builder_.MutableUnpositionedFloats() = space->UnpositionedFloats();

  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
}

bool NGInlineLayoutAlgorithm::CreateLine(
    NGLineInfo* line_info,
    RefPtr<NGInlineBreakToken> break_token) {
  if (Node().IsBidiEnabled())
    BidiReorder(&line_info->Results());

  if (!PlaceItems(line_info, break_token))
    return false;

  // If something has resolved our BFC offset we can place all of the
  // unpositioned floats below the current line.
  if (container_builder_.BfcOffset()) {
    NGLogicalOffset origin_point =
        GetOriginPointForFloats(ContainerBfcOffset(), content_size_);
    PositionPendingFloats(origin_point.block_offset, &container_builder_,
                          MutableConstraintSpace());
  }

  return true;
}

void NGInlineLayoutAlgorithm::BidiReorder(NGInlineItemResults* line_items) {
  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  Vector<UBiDiLevel, 32> levels;
  levels.ReserveInitialCapacity(line_items->size());
  const Vector<NGInlineItem>& items = Node().Items();
  for (const auto& item_result : *line_items)
    levels.push_back(items[item_result.item_index].BidiLevel());
  Vector<int32_t, 32> indices_in_visual_order(line_items->size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  NGInlineItemResults line_items_in_visual_order(line_items->size());
  for (unsigned visual_index = 0; visual_index < indices_in_visual_order.size();
       visual_index++) {
    unsigned logical_index = indices_in_visual_order[visual_index];
    line_items_in_visual_order[visual_index] =
        std::move((*line_items)[logical_index]);
  }

  // Keep Open before Close in the visual order.
  HashMap<LayoutObject*, unsigned> first_index;
  for (unsigned i = 0; i < line_items_in_visual_order.size(); i++) {
    NGInlineItemResult& item_result = line_items_in_visual_order[i];
    const NGInlineItem& item = items[item_result.item_index];
    if (item.Type() != NGInlineItem::kOpenTag &&
        item.Type() != NGInlineItem::kCloseTag) {
      continue;
    }
    auto result = first_index.insert(item.GetLayoutObject(), i);
    if (!result.is_new_entry && item.Type() == NGInlineItem::kOpenTag) {
      std::swap(line_items_in_visual_order[i],
                line_items_in_visual_order[result.stored_value->value]);
    }
  }

  line_items->swap(line_items_in_visual_order);
}

bool NGInlineLayoutAlgorithm::PlaceItems(
    NGLineInfo* line_info,
    RefPtr<NGInlineBreakToken> break_token) {
  NGInlineItemResults* line_items = &line_info->Results();
  const Vector<NGInlineItem>& items = Node().Items();

  const ComputedStyle& line_style = line_info->LineStyle();
  NGLineHeightMetrics line_metrics(line_style, baseline_type_);
  NGLineHeightMetrics line_metrics_with_leading = line_metrics;
  line_metrics_with_leading.AddLeading(line_style.ComputedLineHeightAsFixed());
  NGLineBoxFragmentBuilder line_box(Node());
  line_box.SetWritingMode(ConstraintSpace().WritingMode());

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGTextFragmentBuilder text_builder(Node());
  NGInlineBoxState* box =
      box_states_.OnBeginPlaceItems(&line_style, baseline_type_);

  // Place items from line-left to line-right along with the baseline.
  // Items are already bidi-reordered to the visual order.
  LayoutUnit position;

  for (auto& item_result : *line_items) {
    const NGInlineItem& item = items[item_result.item_index];
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kControl) {
      DCHECK(item.GetLayoutObject()->IsText());
      DCHECK(!box->text_metrics.IsEmpty());
      text_builder.SetSize(
          {item_result.inline_size, box->text_metrics.LineHeight()});
      // Take all used fonts into account if 'line-height: normal'.
      if (box->include_used_fonts && item.Type() == NGInlineItem::kText) {
        box->AccumulateUsedFonts(item, item_result.start_offset,
                                 item_result.end_offset, baseline_type_);
      }
      RefPtr<NGPhysicalTextFragment> text_fragment =
          text_builder.ToTextFragment(item_result.item_index,
                                      item_result.start_offset,
                                      item_result.end_offset);
      line_box.AddChild(std::move(text_fragment), {position, box->text_top});
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = box_states_.OnOpenTag(item, item_result, &line_box, position);
      // Compute text metrics for all inline boxes since even empty inlines
      // influence the line height.
      // https://drafts.csswg.org/css2/visudet.html#line-height
      box->ComputeTextMetrics(*item.Style(), baseline_type_);
      text_builder.SetDirection(box->style->Direction());
      if (ShouldCreateBoxFragment(item, item_result))
        box->SetNeedsBoxFragment(item_result.needs_box_when_empty);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      position += item_result.inline_size;
      if (box->needs_box_fragment || item_result.needs_box_when_empty) {
        if (item_result.needs_box_when_empty)
          box->SetNeedsBoxFragment(true);
        box->SetLineRightForBoxFragment(item, item_result, position);
      }
      box = box_states_.OnCloseTag(item, &line_box, box, baseline_type_);
      continue;
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, *line_info, position,
                              &line_box, &text_builder);
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      // TODO(layout-dev): Report the correct static position for the out of
      // flow descendant. We can't do this here yet as it doesn't know the
      // size of the line box.
      container_builder_.AddOutOfFlowDescendant(
          // Absolute positioning blockifies the box's display type.
          // https://drafts.csswg.org/css-display/#transformations
          {NGBlockNode(ToLayoutBox(item.GetLayoutObject())),
           NGStaticPosition::Create(ConstraintSpace().WritingMode(),
                                    ConstraintSpace().Direction(),
                                    NGPhysicalOffset())});
      continue;
    } else {
      continue;
    }

    position += item_result.inline_size;
  }

  if (line_box.Children().IsEmpty()) {
    return true;  // The line was empty.
  }

  // NGLineBreaker should have determined we need a line box, and that has
  // resolved the BFC offset.
  DCHECK(container_builder_.BfcOffset().has_value());

  box_states_.OnEndPlaceItems(&line_box, baseline_type_, position);

  // The baselines are always placed at pixel boundaries. Not doing so results
  // in incorrect layout of text decorations, most notably underlines.
  LayoutUnit baseline = line_info->LineTop() + line_box.Metrics().ascent;
  baseline = LayoutUnit(baseline.Round());

  // Check if the line fits into the constraint space in block direction.
  LayoutUnit line_bottom = baseline + line_box.Metrics().descent;

  if (!container_builder_.Children().IsEmpty() &&
      ConstraintSpace().AvailableSize().block_size != NGSizeIndefinite &&
      line_bottom > ConstraintSpace().AvailableSize().block_size) {
    return false;
  }

  line_box.SetBreakToken(std::move(break_token));

  // TODO(kojii): Implement flipped line (vertical-lr). In this case, line_top
  // and block_start do not match.

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box.MoveChildrenInBlockDirection(baseline);

  // Compute the offset of the line box.
  LayoutUnit inline_size = position;
  NGLogicalOffset offset(line_info->LineLeft(),
                         baseline - box_states_.LineBoxState().metrics.ascent);
  LayoutUnit available_width = line_info->AvailableWidth();
  if (LayoutUnit text_indent = line_info->TextIndent()) {
    // Move the line box by indent. Negative indents are ink overflow, let the
    // line box overflow from the container box.
    if (IsLtr(Node().BaseDirection()))
      offset.inline_offset += text_indent;
    available_width -= text_indent;
  }
  ApplyTextAlign(line_style.GetTextAlign(line_info->IsLastLine()),
                 &offset.inline_offset, inline_size, available_width);

  line_box.SetInlineSize(inline_size);
  container_builder_.AddChild(line_box.ToLineBoxFragment(), offset);

  max_inline_size_ = std::max(max_inline_size_, inline_size);
  content_size_ = ComputeContentSize(*line_info, line_bottom);

  return true;
}

// TODO(kojii): Currently, this function does not change item_result, but
// when NG paint is enabled, this will std::move() the LayoutResult.
NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info,
    LayoutUnit position,
    NGLineBoxFragmentBuilder* line_box,
    NGTextFragmentBuilder* text_builder) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  const ComputedStyle& style = *item.Style();
  position += item_result->margins.LineLeft(style.Direction());

  NGInlineBoxState* box =
      box_states_.OnOpenTag(item, *item_result, line_box, position);

  // For replaced elements, inline-block elements, and inline-table elements,
  // the height is the height of their margin box.
  // https://drafts.csswg.org/css2/visudet.html#line-height
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(
          item_result->layout_result->PhysicalFragment().Get()));
  LayoutUnit block_size =
      fragment.BlockSize() + item_result->margins.BlockSum();

  // TODO(kojii): Add baseline position to NGPhysicalFragment.
  LayoutBox* layout_box = ToLayoutBox(item.GetLayoutObject());
  LineDirectionMode line_direction_mode =
      IsHorizontalWritingMode() ? LineDirectionMode::kHorizontalLine
                                : LineDirectionMode::kVerticalLine;
  LayoutUnit baseline_offset(layout_box->BaselinePosition(
      baseline_type_, line_info.UseFirstLineStyle(), line_direction_mode));

  NGLineHeightMetrics metrics(baseline_offset, block_size - baseline_offset);
  box->metrics.Unite(metrics);

  // TODO(kojii): Figure out what to do with OOF in NGLayoutResult.
  // Floats are ok because atomic inlines are BFC?

  // TODO(kojii): Try to eliminate the wrapping text fragment and use the
  // |fragment| directly. Currently |CopyFragmentDataToLayoutBlockFlow|
  // requires a text fragment.
  text_builder->SetDirection(style.Direction());
  text_builder->SetSize({fragment.InlineSize(), block_size});
  LayoutUnit line_top = item_result->margins.block_start - metrics.ascent;
  RefPtr<NGPhysicalTextFragment> text_fragment = text_builder->ToTextFragment(
      item_result->item_index, item_result->start_offset,
      item_result->end_offset);
  line_box->AddChild(std::move(text_fragment), {position, line_top});

  return box_states_.OnCloseTag(item, line_box, box, baseline_type_);
}

void NGInlineLayoutAlgorithm::ApplyTextAlign(ETextAlign text_align,
                                             LayoutUnit* line_left,
                                             LayoutUnit inline_size,
                                             LayoutUnit available_width) {
  bool is_base_ltr = IsLtr(Node().BaseDirection());
  // TODO(kojii): Investigate handling trailing spaces for 'white-space:
  // pre|pre-wrap'. Refer to LayoutBlockFlow::UpdateLogicalWidthForAlignment().
  while (true) {
    switch (text_align) {
      case ETextAlign::kLeft:
      case ETextAlign::kWebkitLeft:
        // The direction of the block should determine what happens with wide
        // lines. In particular with RTL blocks, wide lines should still spill
        // out to the left.
        if (!is_base_ltr && inline_size > available_width)
          *line_left -= inline_size - available_width;
        return;
      case ETextAlign::kRight:
      case ETextAlign::kWebkitRight:
        // Wide lines spill out of the block based off direction.
        // So even if text-align is right, if direction is LTR, wide lines
        // should overflow out of the right side of the block.
        if (inline_size < available_width || !is_base_ltr)
          *line_left += available_width - inline_size;
        return;
      case ETextAlign::kCenter:
      case ETextAlign::kWebkitCenter:
        if (is_base_ltr) {
          *line_left +=
              std::max((available_width - inline_size) / 2, LayoutUnit());
        } else if (inline_size <= available_width) {
          *line_left += (available_width - inline_size) / 2;
        } else {
          // In RTL, wide lines should spill out to the left, same as kRight.
          *line_left += available_width - inline_size;
        }
        return;
      case ETextAlign::kStart:
        text_align = is_base_ltr ? ETextAlign::kLeft : ETextAlign::kRight;
        continue;
      case ETextAlign::kEnd:
        text_align = is_base_ltr ? ETextAlign::kRight : ETextAlign::kLeft;
        continue;
      case ETextAlign::kJustify:
        // TODO(kojii): Implement.
        return;
    }
    NOTREACHED();
    return;
  }
}

LayoutUnit NGInlineLayoutAlgorithm::ComputeContentSize(
    const NGLineInfo& line_info,
    LayoutUnit line_bottom) {
  LayoutUnit content_size = line_bottom;

  const Vector<NGInlineItem>& items = Node().Items();
  const NGInlineItemResults& line_items = line_info.Results();
  DCHECK(!line_items.IsEmpty());

  // If the last item was a <br> we need to adjust the content_size to clear
  // floats if specified. The <br> element must be at the back of the item
  // result list as it forces a line to break.
  const NGInlineItemResult& item_result = line_items.back();
  const NGInlineItem& item = items[item_result.item_index];
  const LayoutObject* layout_object = item.GetLayoutObject();

  // layout_object may be null in certain cases, e.g. if it's a kBidiControl.
  if (layout_object && layout_object->IsBR()) {
    NGLogicalOffset bfc_offset =
        ContainerBfcOffset() + NGLogicalOffset(LayoutUnit(), content_size);
    AdjustToClearance(GetClearanceOffset(ConstraintSpace().Exclusions(),
                                         item.Style()->Clear()),
                      &bfc_offset);
    content_size = bfc_offset.block_offset - ContainerBfcOffset().block_offset;
  }

  return content_size;
}

RefPtr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  // Line boxes should start at (0,0).
  // The parent NGBlockLayoutAlgorithm places the anonymous wrapper using the
  // border and padding of the container block.
  content_size_ = LayoutUnit();

  NGLineBreaker line_breaker(Node(), constraint_space_, &container_builder_,
                             BreakToken());
  NGLineInfo line_info;
  while (line_breaker.NextLine(&line_info, {LayoutUnit(), content_size_}))
    CreateLine(&line_info, line_breaker.CreateBreakToken());

  // TODO(kojii): Check if the line box width should be content or available.
  NGLogicalSize size(max_inline_size_, content_size_);
  container_builder_.SetSize(size).SetOverflowSize(size);

  // TODO(crbug.com/716930): We may be an empty LayoutInline due to splitting.
  // Margin struts shouldn't need to be passed through like this once we've
  // removed LayoutInline splitting.
  if (!container_builder_.BfcOffset()) {
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());
  }

  return container_builder_.ToBoxFragment();
}

}  // namespace blink
