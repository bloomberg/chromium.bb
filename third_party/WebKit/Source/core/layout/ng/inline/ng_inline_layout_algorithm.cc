// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

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
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_length_utils.h"
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
          blink::IsHorizontalWritingMode(space.GetWritingMode())) {
  quirks_mode_ = inline_node.InLineHeightQuirksMode();
  unpositioned_floats_ = ConstraintSpace().UnpositionedFloats();

  if (!is_horizontal_writing_mode_)
    baseline_type_ = FontBaseline::kIdeographicBaseline;
}

void NGInlineLayoutAlgorithm::CreateLine(NGLineInfo* line_info,
                                         NGExclusionSpace* exclusion_space) {
  if (Node().IsBidiEnabled())
    BidiReorder(&line_info->Results());

  PlaceItems(line_info, *exclusion_space);
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

void NGInlineLayoutAlgorithm::PlaceItems(
    NGLineInfo* line_info,
    const NGExclusionSpace& exclusion_space) {
  NGInlineItemResults* line_items = &line_info->Results();
  line_box_.clear();

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

  NGTextFragmentBuilder text_builder(Node(),
                                     ConstraintSpace().GetWritingMode());
  Optional<unsigned> list_marker_index;

  // Compute heights of all inline items by placing the dominant baseline at 0.
  // The baseline is adjusted after the height of the line box is computed.
  NGInlineBoxState* box =
      box_states_->OnBeginPlaceItems(&line_style, baseline_type_, quirks_mode_);

  // Place items from line-left to line-right along with the baseline.
  // Items are already bidi-reordered to the visual order.
  LayoutUnit position;

  if (IsRtl(line_info->BaseDirection()) && line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), &position, box,
                          &text_builder);
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
        if (quirks_mode_ && line_box_.IsEmpty())
          box->ActivateTextMetrics();
        DCHECK(!item.TextShapeResult());  // kControl or unit tests.
      }

      text_builder.SetItem(&item_result, box->text_height);
      scoped_refptr<NGPhysicalTextFragment> text_fragment =
          text_builder.ToTextFragment(item_result.item_index,
                                      item_result.start_offset,
                                      item_result.end_offset);
      line_box_.AddChild(std::move(text_fragment), {position, box->text_top});
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = box_states_->OnOpenTag(item, item_result, line_box_, position);
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
      box = box_states_->OnCloseTag(&line_box_, box, baseline_type_);
      continue;
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, *line_info, position);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      list_marker_index = line_box_.size();
      PlaceListMarker(item, &item_result, *line_info);
      DCHECK_GT(line_box_.size(), list_marker_index.value());
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));
      container_builder_.AddInlineOutOfFlowChildCandidate(
          node, NGLogicalOffset(position, LayoutUnit()),
          line_info->BaseDirection());
      continue;
    } else {
      continue;
    }

    position += item_result.inline_size;
  }

  if (line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), &position, box,
                          &text_builder);
  }

  if (line_box_.IsEmpty()) {
    return;  // The line was empty.
  }

  box_states_->OnEndPlaceItems(&line_box_, baseline_type_, position);
  const NGLineHeightMetrics& line_box_metrics =
      box_states_->LineBoxState().metrics;
  DCHECK(!line_box_metrics.IsEmpty());

  NGBfcOffset line_bfc_offset(line_info->LineBfcOffset());

  // TODO(kojii): Implement flipped line (vertical-lr). In this case, line_top
  // and block_start do not match.

  // Up until this point, children are placed so that the dominant baseline is
  // at 0. Move them to the final baseline position, and set the logical top of
  // the line box to the line top.
  line_box_.MoveInBlockDirection(line_box_metrics.ascent);

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  LayoutUnit inline_size = position.ClampNegativeToZero();

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  if (text_align != ETextAlign::kJustify) {
    ApplyTextAlign(*line_info, text_align, &line_bfc_offset.line_offset,
                   inline_size);
  }

  if (list_marker_index.has_value()) {
    NGListLayoutAlgorithm::SetListMarkerPosition(
        constraint_space_, *line_info, inline_size, list_marker_index.value(),
        &line_box_);
  }

  container_builder_.AddChildren(line_box_);
  container_builder_.SetInlineSize(inline_size);
  container_builder_.SetMetrics(line_box_metrics);
  container_builder_.SetBfcOffset(line_bfc_offset);
}

// Place a generated content that does not exist in DOM nor in LayoutObject
// tree.
void NGInlineLayoutAlgorithm::PlaceGeneratedContent(
    scoped_refptr<const ShapeResult> shape_result,
    scoped_refptr<const ComputedStyle> style,
    LayoutUnit* position,
    NGInlineBoxState* box,
    NGTextFragmentBuilder* text_builder) {
  if (box->CanAddTextOfStyle(*style)) {
    PlaceText(std::move(shape_result), std::move(style), position, box,
              text_builder);
  } else {
    scoped_refptr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(*style,
                                                       EDisplay::kInline);
    NGInlineBoxState* box = box_states_->OnOpenTag(*text_style, line_box_);
    box->ComputeTextMetrics(*text_style, baseline_type_, false);
    PlaceText(std::move(shape_result), std::move(style), position, box,
              text_builder);
    box_states_->OnCloseTag(&line_box_, box, baseline_type_);
  }
}

void NGInlineLayoutAlgorithm::PlaceText(
    scoped_refptr<const ShapeResult> shape_result,
    scoped_refptr<const ComputedStyle> style,
    LayoutUnit* position,
    NGInlineBoxState* box,
    NGTextFragmentBuilder* text_builder) {
  unsigned start_offset = shape_result->StartIndexForResult();
  unsigned end_offset = shape_result->EndIndexForResult();
  LayoutUnit inline_size = shape_result->SnappedWidth();
  text_builder->SetText(std::move(style), std::move(shape_result),
                        {inline_size, box->text_height});
  scoped_refptr<NGPhysicalTextFragment> text_fragment =
      text_builder->ToTextFragment(std::numeric_limits<unsigned>::max(),
                                   start_offset, end_offset);
  line_box_.AddChild(std::move(text_fragment), {*position, box->text_top});
  *position += inline_size;
}

NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info,
    LayoutUnit position) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  const ComputedStyle& style = *item.Style();
  position += item_result->margins.LineLeft(style.Direction());

  NGInlineBoxState* box =
      box_states_->OnOpenTag(item, *item_result, line_box_, position);

  PlaceLayoutResult(item_result, position, box);

  return box_states_->OnCloseTag(&line_box_, box, baseline_type_);
}

// Place a NGLayoutResult into the line box.
void NGInlineLayoutAlgorithm::PlaceLayoutResult(NGInlineItemResult* item_result,
                                                LayoutUnit position,
                                                NGInlineBoxState* box) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->layout_result->PhysicalFragment());
  DCHECK(item_result->item->Style());
  const ComputedStyle& style = *item_result->item->Style();
  NGBoxFragment fragment(
      ConstraintSpace().GetWritingMode(),
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
    NGTextFragmentBuilder text_builder(Node(),
                                       ConstraintSpace().GetWritingMode());
    text_builder.SetAtomicInline(&style,
                                 {fragment.InlineSize(), metrics.LineHeight()});
    scoped_refptr<NGPhysicalTextFragment> text_fragment =
        text_builder.ToTextFragment(item_result->item_index,
                                    item_result->start_offset,
                                    item_result->end_offset);
    line_box_.AddChild(std::move(text_fragment), {position, line_top});
    // We need the box fragment as well to compute VisualRect() correctly.
  }

  line_box_.AddChild(std::move(item_result->layout_result),
                     {position, line_top});
}

// Place a list marker.
void NGInlineLayoutAlgorithm::PlaceListMarker(const NGInlineItem& item,
                                              NGInlineItemResult* item_result,
                                              const NGLineInfo& line_info) {
  if (quirks_mode_)
    box_states_->LineBoxState().ActivateTextMetrics();

  item_result->layout_result =
      NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
          .LayoutAtomicInline(constraint_space_, line_info.UseFirstLineStyle());
  DCHECK(item_result->layout_result->PhysicalFragment());

  // The inline position is adjusted later, when we knew the line width.
  PlaceLayoutResult(item_result, LayoutUnit(), nullptr);
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
bool NGInlineLayoutAlgorithm::ApplyJustify(NGLineInfo* line_info) {
  LayoutUnit inline_size;
  LayoutUnit available_width = line_info->AvailableWidth();
  if (line_info->LineEndShapeResult())
    available_width -= line_info->LineEndShapeResult()->SnappedWidth();
  LayoutUnit expansion = available_width - line_info->Width();
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
      scoped_refptr<ShapeResult> shape_result =
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
    LayoutUnit line_height) {
  LayoutUnit content_size = line_height;

  const NGInlineItemResults& line_items = line_info.Results();
  if (line_items.IsEmpty())
    return content_size;

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

scoped_refptr<NGLayoutResult> NGInlineLayoutAlgorithm::Layout() {
  std::unique_ptr<NGExclusionSpace> initial_exclusion_space(
      std::make_unique<NGExclusionSpace>(ConstraintSpace().ExclusionSpace()));

  bool is_empty_inline = Node().IsEmptyInline();

  if (!is_empty_inline) {
    DCHECK(ConstraintSpace().UnpositionedFloats().IsEmpty());
    DCHECK(ConstraintSpace().MarginStrut().IsEmpty());
    container_builder_.SetBfcOffset(ConstraintSpace().BfcOffset());
  }

  // In order to get the correct list of layout opportunities, we need to
  // position any "leading" items (floats) within the exclusion space first.
  unsigned handled_item_index =
      PositionLeadingItems(initial_exclusion_space.get());

  // If we are an empty inline, we don't have to run the full algorithm, we can
  // return now as we should have positioned all of our floats.
  if (is_empty_inline) {
    DCHECK_EQ(handled_item_index, Node().Items().size());

    container_builder_.SwapPositionedFloats(&positioned_floats_);
    container_builder_.SwapUnpositionedFloats(&unpositioned_floats_);
    container_builder_.SetEndMarginStrut(ConstraintSpace().MarginStrut());
    container_builder_.SetExclusionSpace(std::move(initial_exclusion_space));

    Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
    container_builder_.GetAndClearOutOfFlowDescendantCandidates(
        &descendant_candidates);
    for (auto& descendant : descendant_candidates)
      container_builder_.AddOutOfFlowDescendant(descendant);

    return container_builder_.ToLineBoxFragment();
  }

  DCHECK(container_builder_.BfcOffset());

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  Vector<NGLayoutOpportunity> opportunities =
      initial_exclusion_space->AllLayoutOpportunities(
          ConstraintSpace().BfcOffset(), ConstraintSpace().AvailableSize());

  Vector<NGPositionedFloat> positioned_floats;
  DCHECK(unpositioned_floats_.IsEmpty());

  std::unique_ptr<NGExclusionSpace> exclusion_space;
  NGInlineBreakToken* break_token = BreakToken();

  for (const auto& opportunity : opportunities) {
    // Copy the state stack from the unfinished break token if provided. This
    // enforces the layout inputs immutability constraint. If we weren't
    // provided with a break token we just create an empty state stack.
    box_states_ = break_token ? std::make_unique<NGInlineLayoutStateStack>(
                                    break_token->StateStack())
                              : std::make_unique<NGInlineLayoutStateStack>();

    // Reset any state that may have been modified in a previous pass.
    positioned_floats.clear();
    unpositioned_floats_.clear();
    container_builder_.Reset();
    exclusion_space =
        std::make_unique<NGExclusionSpace>(*initial_exclusion_space);

    NGLineInfo line_info;
    NGLineBreaker line_breaker(Node(), constraint_space_, &positioned_floats,
                               &unpositioned_floats_, exclusion_space.get(),
                               handled_item_index, break_token);

    // TODO(ikilpatrick): Does this always succeed when we aren't an empty
    // inline?
    if (!line_breaker.NextLine(opportunity, &line_info))
      break;

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size,
    // continue to the next opportunity.
    if (line_info.Width() > opportunity.InlineSize() &&
        opportunity.InlineSize() !=
            ConstraintSpace().AvailableSize().inline_size)
      continue;

    CreateLine(&line_info, exclusion_space.get());

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity.
    LayoutUnit block_size = container_builder_.ComputeBlockSize();
    if (block_size > opportunity.BlockSize())
      continue;

    LayoutUnit line_height =
        container_builder_.Metrics().LineHeight().ClampNegativeToZero();

    // Success!
    positioned_floats_.AppendVector(positioned_floats);
    container_builder_.SetBreakToken(
        line_breaker.CreateBreakToken(std::move(box_states_)));

    // Place any remaining floats which couldn't fit on the line.
    PositionPendingFloats(line_height, exclusion_space.get());

    // A <br clear=both> will strech the line-box height, such that the
    // block-end edge will clear any floats.
    // TODO(ikilpatrick): Move this into ng_block_layout_algorithm.
    container_builder_.SetBlockSize(
        ComputeContentSize(line_info, *exclusion_space, line_height));

    break;
  }

  // We shouldn't have any unpositioned floats if we aren't empty.
  DCHECK(unpositioned_floats_.IsEmpty());
  container_builder_.SwapPositionedFloats(&positioned_floats_);
  container_builder_.SetExclusionSpace(
      exclusion_space ? std::move(exclusion_space)
                      : std::move(initial_exclusion_space));

  Vector<NGOutOfFlowPositionedDescendant> descendant_candidates;
  container_builder_.GetAndClearOutOfFlowDescendantCandidates(
      &descendant_candidates);
  for (auto& descendant : descendant_candidates)
    container_builder_.AddOutOfFlowDescendant(descendant);
  return container_builder_.ToLineBoxFragment();
}

// This positions any "leading" floats within the given exclusion space.
// If we are also an empty inline, it will add any out-of-flow descendants.
// TODO(ikilpatrick): Do we need to always add the OOFs here?
unsigned NGInlineLayoutAlgorithm::PositionLeadingItems(
    NGExclusionSpace* exclusion_space) {
  const Vector<NGInlineItem>& items = Node().Items();
  bool is_empty_inline = Node().IsEmptyInline();
  LayoutUnit bfc_line_offset = ConstraintSpace().BfcOffset().line_offset;

  unsigned index = BreakToken() ? BreakToken()->ItemIndex() : 0;
  for (; index < items.size(); ++index) {
    const auto& item = items[index];

    if (item.Type() == NGInlineItem::kFloating) {
      NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));
      NGBoxStrut margins =
          ComputeMarginsForContainer(ConstraintSpace(), node.Style());

      unpositioned_floats_.push_back(NGUnpositionedFloat::Create(
          ConstraintSpace().AvailableSize(),
          ConstraintSpace().PercentageResolutionSize(), bfc_line_offset,
          bfc_line_offset, margins, node, /* break_token */ nullptr));
    } else if (is_empty_inline &&
               item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));
      container_builder_.AddInlineOutOfFlowChildCandidate(
          node, NGLogicalOffset(), Style().Direction());
    }

    // Abort if we've found something that makes this a non-empty inline.
    if (!item.IsEmptyItem()) {
      DCHECK(!is_empty_inline);
      break;
    }
  }

  if (ConstraintSpace().FloatsBfcOffset() || container_builder_.BfcOffset())
    PositionPendingFloats(/* content_size */ LayoutUnit(), exclusion_space);

  return index;
}

void NGInlineLayoutAlgorithm::PositionPendingFloats(
    LayoutUnit content_size,
    NGExclusionSpace* exclusion_space) {
  DCHECK(container_builder_.BfcOffset() || ConstraintSpace().FloatsBfcOffset())
      << "The parent BFC offset should be known here";

  NGBfcOffset bfc_offset = container_builder_.BfcOffset()
                               ? container_builder_.BfcOffset().value()
                               : ConstraintSpace().FloatsBfcOffset().value();

  LayoutUnit origin_block_offset = bfc_offset.block_offset + content_size;
  LayoutUnit from_block_offset = bfc_offset.block_offset;

  const auto positioned_floats =
      PositionFloats(origin_block_offset, from_block_offset,
                     unpositioned_floats_, ConstraintSpace(), exclusion_space);

  positioned_floats_.AppendVector(positioned_floats);
  unpositioned_floats_.clear();
}

}  // namespace blink
