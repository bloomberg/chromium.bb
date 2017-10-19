// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include "core/layout/ng/inline/ng_baseline.h"
#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_line_box_fragment.h"
#include "core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "core/layout/ng/inline/ng_line_breaker.h"
#include "core/layout/ng/inline/ng_list_layout_algorithm.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/inline/ng_text_fragment_builder.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_space_utils.h"
#include "core/layout/ng/ng_unpositioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"

namespace blink {
namespace {

inline bool ShouldCreateBoxFragment(const NGInlineItem& item,
                                    const NGInlineItemResult& item_result) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  // TODO(kojii): We might need more conditions to create box fragments.
  return style.HasBoxDecorationBackground() || style.HasOutline() ||
         item_result.needs_box_when_empty;
}

NGBfcOffset GetOriginPointForFloats(const NGBfcOffset& container_bfc_offset,
                                    LayoutUnit content_size) {
  NGBfcOffset origin_point = container_bfc_offset;
  origin_point.block_offset += content_size;
  return origin_point;
}

}  // namespace

NGInlineLayoutAlgorithm::NGInlineLayoutAlgorithm(
    NGInlineNode inline_node,
    const NGConstraintSpace& space,
    NGInlineBreakToken* break_token)
    : NGLayoutAlgorithm(
          inline_node,
          ComputedStyle::CreateAnonymousStyleWithDisplay(inline_node.Style(),
                                                         EDisplay::kBlock),
          space,
          // Use LTR direction since inline layout handles bidi by itself and
          // lays out in visual order.
          TextDirection::kLtr,
          break_token),
      is_horizontal_writing_mode_(
          blink::IsHorizontalWritingMode(space.WritingMode())) {
  quirks_mode_ = !inline_node.GetLayoutObject()->GetDocument().InNoQuirksMode();
  unpositioned_floats_ = ConstraintSpace().UnpositionedFloats();

  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
}

bool NGInlineLayoutAlgorithm::CreateLine(
    NGLineInfo* line_info,
    NGExclusionSpace* exclusion_space,
    RefPtr<NGInlineBreakToken> break_token) {
  if (Node().IsBidiEnabled())
    BidiReorder(&line_info->Results());

  if (!PlaceItems(line_info, *exclusion_space, break_token))
    return false;

  // If something has resolved our BFC offset we can place all of the
  // unpositioned floats below the current line.
  if (container_builder_.BfcOffset()) {
    NGBfcOffset origin_point =
        GetOriginPointForFloats(ContainerBfcOffset(), content_size_);
    PositionPendingFloats(ConstraintSpace(), origin_point.block_offset,
                          &container_builder_, &unpositioned_floats_,
                          exclusion_space);
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
  for (const auto& item_result : *line_items)
    levels.push_back(item_result.item->BidiLevelForReorder());
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
    const NGInlineItem& item = *item_result.item;
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
    const NGExclusionSpace& exclusion_space,
    RefPtr<NGInlineBreakToken> break_token) {
  NGInlineItemResults* line_items = &line_info->Results();

  // Apply justification before placing items, because it affects size/position
  // of items, which are needed to compute inline static positions.
  const ComputedStyle& line_style = line_info->LineStyle();
  ETextAlign text_align = line_style.GetTextAlign(line_info->IsLastLine());
  if (text_align == ETextAlign::kJustify) {
    if (!ApplyJustify(line_info))
      text_align = ETextAlign::kStart;
  }

  NGLineHeightMetrics line_metrics(line_style, baseline_type_);
  NGLineHeightMetrics line_metrics_with_leading = line_metrics;
  line_metrics_with_leading.AddLeading(line_style.ComputedLineHeightAsFixed());
  NGLineBoxFragmentBuilder line_box(Node(), &line_style,
                                    ConstraintSpace().WritingMode());
  NGTextFragmentBuilder text_builder(Node(), ConstraintSpace().WritingMode());
  Optional<unsigned> list_marker_index;

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGInlineBoxState* box =
      box_states_.OnBeginPlaceItems(&line_style, baseline_type_, quirks_mode_);

  // Place items from line-left to line-right along with the baseline.
  // Items are already bidi-reordered to the visual order.
  LayoutUnit position;

  if (IsRtl(line_info->BaseDirection()) && line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), &position, box,
                          &text_builder, &line_box);
  }

  for (auto& item_result : *line_items) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kControl) {
      DCHECK(item.GetLayoutObject()->IsText() ||
             item.GetLayoutObject()->IsLayoutNGListItem());
      DCHECK(!box->text_metrics.IsEmpty());
      if (item_result.shape_result) {
        if (quirks_mode_)
          box->ActivateTextMetrics();
        // Take all used fonts into account if 'line-height: normal'.
        if (box->include_used_fonts && item.Type() == NGInlineItem::kText) {
          box->AccumulateUsedFonts(item_result.shape_result.get(),
                                   baseline_type_);
        }
      } else {
        if (quirks_mode_ && line_box.Children().IsEmpty())
          box->ActivateTextMetrics();
        DCHECK(!item.TextShapeResult());  // kControl or unit tests.
      }

      text_builder.SetItem(&item_result, box->text_metrics.LineHeight());
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
      box->ComputeTextMetrics(*item.Style(), baseline_type_, quirks_mode_);
      if (quirks_mode_ && item_result.needs_box_when_empty)
        box->ActivateTextMetrics();
      if (ShouldCreateBoxFragment(item, item_result))
        box->SetNeedsBoxFragment(item_result.needs_box_when_empty);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      position += item_result.inline_size;
      if (box->needs_box_fragment || item_result.needs_box_when_empty) {
        if (item_result.needs_box_when_empty)
          box->SetNeedsBoxFragment(true);
        box->SetLineRightForBoxFragment(item, item_result, position);
        if (quirks_mode_)
          box->ActivateTextMetrics();
      }
      box = box_states_.OnCloseTag(&line_box, box, baseline_type_);
      continue;
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, *line_info, position,
                              &line_box);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      list_marker_index = line_box.Children().size();
      PlaceListMarker(item, &item_result, *line_info, &line_box);
      DCHECK_GT(line_box.Children().size(), list_marker_index.value());
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

  if (line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), &position, box,
                          &text_builder, &line_box);
  }

  if (line_box.Children().IsEmpty()) {
    return true;  // The line was empty.
  }

  // NGLineBreaker should have determined we need a line box, and that has
  // resolved the BFC offset.
  DCHECK(container_builder_.BfcOffset().has_value());

  box_states_.OnEndPlaceItems(&line_box, baseline_type_, position);

  // Check if the line fits into the constraint space in block direction.
  NGLogicalOffset line_offset(line_info->LineOffset());
  LayoutUnit line_bottom =
      line_offset.block_offset + line_box.Metrics().LineHeight();
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
  line_box.MoveChildrenInBlockDirection(line_box.Metrics().ascent);

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  LayoutUnit inline_size = position;
  if (text_align != ETextAlign::kJustify) {
    ApplyTextAlign(*line_info, text_align, &line_offset.inline_offset,
                   inline_size);
  }

  if (list_marker_index.has_value()) {
    NGListLayoutAlgorithm::SetListMarkerPosition(
        constraint_space_, *line_info, inline_size, list_marker_index.value(),
        &line_box);
  }

  line_box.SetInlineSize(inline_size);
  container_builder_.AddChild(line_box.ToLineBoxFragment(), line_offset);

  max_inline_size_ = std::max(max_inline_size_, inline_size);
  content_size_ = ComputeContentSize(*line_info, exclusion_space, line_bottom);

  return true;
}

// Place a generated content that does not exist in DOM nor in LayoutObject
// tree.
void NGInlineLayoutAlgorithm::PlaceGeneratedContent(
    RefPtr<const ShapeResult> shape_result,
    RefPtr<const ComputedStyle> style,
    LayoutUnit* position,
    NGInlineBoxState* box,
    NGTextFragmentBuilder* text_builder,
    NGLineBoxFragmentBuilder* line_box) {
  if (box->CanAddTextOfStyle(*style)) {
    PlaceText(std::move(shape_result), std::move(style), position, box,
              text_builder, line_box);
  } else {
    RefPtr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(*style,
                                                       EDisplay::kInline);
    NGInlineBoxState* box = box_states_.OnOpenTag(*text_style, line_box);
    box->ComputeTextMetrics(*text_style, baseline_type_, false);
    PlaceText(std::move(shape_result), std::move(style), position, box,
              text_builder, line_box);
    box_states_.OnCloseTag(line_box, box, baseline_type_);
  }
}

void NGInlineLayoutAlgorithm::PlaceText(RefPtr<const ShapeResult> shape_result,
                                        RefPtr<const ComputedStyle> style,
                                        LayoutUnit* position,
                                        NGInlineBoxState* box,
                                        NGTextFragmentBuilder* text_builder,
                                        NGLineBoxFragmentBuilder* line_box) {
  LayoutUnit inline_size = shape_result->SnappedWidth();
  text_builder->SetText(std::move(style), std::move(shape_result), inline_size,
                        box->text_metrics.LineHeight());
  RefPtr<NGPhysicalTextFragment> text_fragment =
      text_builder->ToTextFragment(std::numeric_limits<unsigned>::max(), 0, 0);
  line_box->AddChild(std::move(text_fragment), {*position, box->text_top});
  *position += inline_size;
}

NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info,
    LayoutUnit position,
    NGLineBoxFragmentBuilder* line_box) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  const ComputedStyle& style = *item.Style();
  position += item_result->margins.LineLeft(style.Direction());

  NGInlineBoxState* box =
      box_states_.OnOpenTag(item, *item_result, line_box, position);

  PlaceLayoutResult(item_result, position, box, line_box);

  return box_states_.OnCloseTag(line_box, box, baseline_type_);
}

// Place a NGLayoutResult into the line box.
void NGInlineLayoutAlgorithm::PlaceLayoutResult(
    NGInlineItemResult* item_result,
    LayoutUnit position,
    NGInlineBoxState* box,
    NGLineBoxFragmentBuilder* line_box) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->layout_result->PhysicalFragment());
  DCHECK(item_result->item->Style());
  const ComputedStyle& style = *item_result->item->Style();
  NGBoxFragment fragment(
      ConstraintSpace().WritingMode(),
      ToNGPhysicalBoxFragment(*item_result->layout_result->PhysicalFragment()));
  NGLineHeightMetrics metrics = fragment.BaselineMetrics(
      {NGBaselineAlgorithmType::kAtomicInline, baseline_type_},
      ConstraintSpace());
  if (box)
    box->metrics.Unite(metrics);

  LayoutUnit line_top = item_result->margins.block_start - metrics.ascent;
  if (!RuntimeEnabledFeatures::LayoutNGPaintFragmentsEnabled()) {
    // |CopyFragmentDataToLayoutBox| needs to know if a box fragment is an
    // atomic inline, and its item_index. Add a text fragment as a marker.
    NGTextFragmentBuilder text_builder(Node(), ConstraintSpace().WritingMode());
    text_builder.SetAtomicInline(&style, fragment.InlineSize(),
                                 metrics.LineHeight());
    RefPtr<NGPhysicalTextFragment> text_fragment = text_builder.ToTextFragment(
        item_result->item_index, item_result->start_offset,
        item_result->end_offset);
    line_box->AddChild(std::move(text_fragment), {position, line_top});
    // We need the box fragment as well to compute VisualRect() correctly.
  }

  line_box->AddChild(std::move(item_result->layout_result),
                     {position, line_top});
}

// Place a list marker.
void NGInlineLayoutAlgorithm::PlaceListMarker(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info,
    NGLineBoxFragmentBuilder* line_box) {
  if (quirks_mode_)
    box_states_.LineBoxState().ActivateTextMetrics();

  item_result->layout_result =
      NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
          .LayoutAtomicInline(constraint_space_, line_info.UseFirstLineStyle());
  DCHECK(item_result->layout_result->PhysicalFragment());

  // The inline position is adjusted later, when we knew the line width.
  PlaceLayoutResult(item_result, LayoutUnit(), nullptr, line_box);
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
bool NGInlineLayoutAlgorithm::ApplyJustify(NGLineInfo* line_info) {
  LayoutUnit inline_size;
  for (const NGInlineItemResult& item_result : line_info->Results())
    inline_size += item_result.inline_size;
  LayoutUnit available_width = line_info->AvailableWidth();
  if (line_info->LineEndShapeResult())
    available_width -= line_info->LineEndShapeResult()->SnappedWidth();
  LayoutUnit expansion = available_width - inline_size;
  if (expansion <= 0)
    return false;  // no expansion is needed.

  // Construct the line text to compute spacing for.
  String line_text =
      Node().Text(line_info->StartOffset(), line_info->EndOffset()).ToString();

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  const NGInlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.text_end_effect == NGTextEndEffect::kHyphen)
    line_text.append(last_item_result.item->Style()->HyphenString());

  // Compute the spacing to justify.
  ShapeResultSpacing<String> spacing(line_text);
  spacing.SetExpansion(expansion, line_info->BaseDirection(),
                       line_info->LineStyle().GetTextJustify());
  if (!spacing.HasExpansion())
    return false;  // no expansion opportunities exist.

  for (NGInlineItemResult& item_result : line_info->Results()) {
    if (item_result.shape_result) {
      // Mutate the existing shape result if only used here, if not create a
      // copy.
      RefPtr<ShapeResult> shape_result =
          item_result.shape_result->MutableUnique();
      DCHECK_GE(item_result.start_offset, line_info->StartOffset());
      // |shape_result| has more characters if it's hyphenated.
      DCHECK(item_result.text_end_effect != NGTextEndEffect::kNone ||
             shape_result->NumCharacters() ==
                 item_result.end_offset - item_result.start_offset);
      LayoutUnit size_before_justify = item_result.inline_size;
      shape_result->ApplySpacing(
          spacing, item_result.start_offset - line_info->StartOffset() -
                       shape_result->StartIndexForResult());
      item_result.inline_size = shape_result->SnappedWidth();
      item_result.expansion =
          (item_result.inline_size - size_before_justify).ToInt();
      item_result.shape_result = std::move(shape_result);
    } else {
      // TODO(kojii): Implement atomic inline.
    }
  }
  return true;
}

void NGInlineLayoutAlgorithm::ApplyTextAlign(const NGLineInfo& line_info,
                                             ETextAlign text_align,
                                             LayoutUnit* line_left,
                                             LayoutUnit inline_size) {
  bool is_base_ltr = IsLtr(line_info.BaseDirection());
  LayoutUnit available_width = line_info.AvailableWidth();
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
        // Justification is applied in earlier phase, see PlaceItems().
        NOTREACHED();
        return;
    }
    NOTREACHED();
    return;
  }
}

LayoutUnit NGInlineLayoutAlgorithm::ComputeContentSize(
    const NGLineInfo& line_info,
    const NGExclusionSpace& exclusion_space,
    LayoutUnit line_bottom) {
  LayoutUnit content_size = line_bottom;

  const NGInlineItemResults& line_items = line_info.Results();
  DCHECK(!line_items.IsEmpty());

  // If the last item was a <br> we need to adjust the content_size to clear
  // floats if specified. The <br> element must be at the back of the item
  // result list as it forces a line to break.
  const NGInlineItemResult& item_result = line_items.back();
  DCHECK(item_result.item);
  const NGInlineItem& item = *item_result.item;
  const LayoutObject* layout_object = item.GetLayoutObject();

  // layout_object may be null in certain cases, e.g. if it's a kBidiControl.
  if (layout_object && layout_object->IsBR()) {
    NGBfcOffset bfc_offset = {ContainerBfcOffset().line_offset,
                              ContainerBfcOffset().block_offset + content_size};
    AdjustToClearance(exclusion_space.ClearanceOffset(item.Style()->Clear()),
                      &bfc_offset);
    content_size = bfc_offset.block_offset - ContainerBfcOffset().block_offset;
  }

  return content_size;
}

// Add a baseline from a child line box fragment.
// @return false if the specified child is not a line box.
bool NGInlineLayoutAlgorithm::AddBaseline(const NGBaselineRequest& request,
                                          const NGPhysicalFragment* child,
                                          LayoutUnit child_offset) {
  if (!child->IsLineBox())
    return false;

  const NGPhysicalLineBoxFragment* line_box =
      ToNGPhysicalLineBoxFragment(child);
  LayoutUnit offset = line_box->BaselinePosition(request.baseline_type);
  container_builder_.AddBaseline(request, offset + child_offset);
  return true;
}

// Compute requested baselines from child line boxes.
void NGInlineLayoutAlgorithm::PropagateBaselinesFromChildren() {
  const Vector<NGBaselineRequest>& requests =
      ConstraintSpace().BaselineRequests();
  if (requests.IsEmpty())
    return;

  for (const auto& request : requests) {
    switch (request.algorithm_type) {
      case NGBaselineAlgorithmType::kAtomicInline:
        // Propagate from the last line box.
        for (unsigned i = container_builder_.Children().size(); i--;) {
          if (AddBaseline(request, container_builder_.Children()[i].get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
      case NGBaselineAlgorithmType::kFirstLine:
        // Propagate from the first line box.
        for (unsigned i = 0; i < container_builder_.Children().size(); i++) {
          if (AddBaseline(request, container_builder_.Children()[i].get(),
                          container_builder_.Offsets()[i].block_offset))
            break;
        }
        break;
    }
  }
}

RefPtr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  // Line boxes should start at (0,0).
  // The parent NGBlockLayoutAlgorithm places the anonymous wrapper using the
  // border and padding of the container block.
  content_size_ = LayoutUnit();

  // We can resolve our BFC offset if we aren't an empty inline.
  if (!Node().IsEmptyInline()) {
    DCHECK(!container_builder_.BfcOffset());
    LayoutUnit bfc_block_offset = constraint_space_.BfcOffset().block_offset +
                                  constraint_space_.MarginStrut().Sum();
    MaybeUpdateFragmentBfcOffset(constraint_space_, bfc_block_offset,
                                 &container_builder_);

    // If we have unpositioned floats from a previous sibling, we need to abort
    // our layout, and tell our parent that we now know our BFC offset.
    if (!unpositioned_floats_.IsEmpty()) {
      container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
      return container_builder_.Abort(NGLayoutResult::kBfcOffsetResolved);
    }
  }

  RefPtr<NGInlineBreakToken> break_token = BreakToken();
  std::unique_ptr<NGExclusionSpace> exclusion_space(
      WTF::MakeUnique<NGExclusionSpace>(ConstraintSpace().ExclusionSpace()));
  NGLineInfo line_info;
  while (!break_token || !break_token->IsFinished()) {
    NGLineBreaker line_breaker(Node(), constraint_space_, &container_builder_,
                               &unpositioned_floats_, break_token.get());
    if (!line_breaker.NextLine({LayoutUnit(), content_size_}, *exclusion_space,
                               &line_info))
      break;

    break_token = line_breaker.CreateBreakToken();
    CreateLine(&line_info, line_breaker.ExclusionSpace(), break_token);

    exclusion_space =
        WTF::MakeUnique<NGExclusionSpace>(*line_breaker.ExclusionSpace());
  }

  // Place any remaining floats which couldn't fit on the previous line.
  // TODO(ikilpatrick): This is duplicated from CreateLine, but flushes any
  // floats if we didn't create a line-box. Refactor this such that this isn't
  // needed.
  if (container_builder_.BfcOffset()) {
    NGBfcOffset origin_point =
        GetOriginPointForFloats(ContainerBfcOffset(), content_size_);
    PositionPendingFloats(ConstraintSpace(), origin_point.block_offset,
                          &container_builder_, &unpositioned_floats_,
                          exclusion_space.get());
  }

  // TODO(kojii): Check if the line box width should be content or available.
  container_builder_.SetInlineSize(max_inline_size_);
  container_builder_.SetBlockSize(content_size_);
  container_builder_.SetIntrinsicBlockSize(content_size_);

  // TODO(crbug.com/716930): We may be an empty LayoutInline due to splitting.
  // Margin struts shouldn't need to be passed through like this once we've
  // removed LayoutInline splitting.
  if (!container_builder_.BfcOffset()) {
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());
  }

  // If we've got any unpositioned floats here, we must be an empty inline
  // without a BFC offset. We need to pass our unpositioned floats to our next
  // sibling.
  if (!unpositioned_floats_.IsEmpty()) {
    DCHECK(Node().IsEmptyInline());
    DCHECK(!container_builder_.BfcOffset());
    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
  }

  container_builder_.SetExclusionSpace(std::move(exclusion_space));

  PropagateBaselinesFromChildren();

  return container_builder_.ToBoxFragment();
}

}  // namespace blink
