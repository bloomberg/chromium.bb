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

// Represents a data struct that are needed for 'text-align' and justifications.
struct NGLineAlign {
  STACK_ALLOCATED();
  NGLineAlign(const NGLineInfo&);
  NGLineAlign() = delete;

  LayoutUnit space;
  unsigned end_offset;
};

NGLineAlign::NGLineAlign(const NGLineInfo& line_info) {
  space = line_info.AvailableWidth() - line_info.Width();

  // Eliminate trailing spaces from the alignment space.
  const NGInlineItemResults& item_results = line_info.Results();
  for (auto it = item_results.rbegin(); it != item_results.rend(); ++it) {
    const NGInlineItemResult& item_result = *it;
    if (!item_result.has_only_trailing_spaces) {
      end_offset = item_result.end_offset;
      return;
    }
    space += item_result.inline_size;
  }

  // An empty line, or only trailing spaces.
  DCHECK_EQ(space, line_info.AvailableWidth());
  end_offset = line_info.StartOffset();
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

  if (IsRtl(line_info->BaseDirection()) && line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), box,
                          &text_builder);
  }

  for (auto& item_result : *line_items) {
    DCHECK(item_result.item);
    const NGInlineItem& item = *item_result.item;
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kControl) {
      if (!item.GetLayoutObject()) {
        // TODO(kojii): Add a flag to NGInlineItem for this case.
        continue;
      }
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
        if (quirks_mode_ && !box->HasMetrics())
          box->ActivateTextMetrics();
        DCHECK(!item.TextShapeResult());  // kControl or unit tests.
      }

      text_builder.SetItem(&item_result, box->text_height);
      scoped_refptr<NGPhysicalTextFragment> text_fragment =
          text_builder.ToTextFragment(item_result.item_index,
                                      item_result.start_offset,
                                      item_result.end_offset);
      line_box_.AddChild(std::move(text_fragment), box->text_top,
                         item_result.inline_size, item.BidiLevel());
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      box = box_states_->OnOpenTag(item, item_result, line_box_);
      // Compute text metrics for all inline boxes since even empty inlines
      // influence the line height.
      // https://drafts.csswg.org/css2/visudet.html#line-height
      box->ComputeTextMetrics(*item.Style(), baseline_type_, quirks_mode_);
      if (quirks_mode_ && item_result.needs_box_when_empty)
        box->ActivateTextMetrics();
      if (ShouldCreateBoxFragment(item, item_result))
        box->SetNeedsBoxFragment(item_result.needs_box_when_empty);
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      if (box->needs_box_fragment || item_result.needs_box_when_empty) {
        if (item_result.needs_box_when_empty)
          box->SetNeedsBoxFragment(true);
        box->SetLineRightForBoxFragment(item, item_result);
        if (quirks_mode_)
          box->ActivateTextMetrics();
      }
      box = box_states_->OnCloseTag(&line_box_, box, baseline_type_);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      box = PlaceAtomicInline(item, &item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      list_marker_index = line_box_.size();
      PlaceListMarker(item, &item_result, *line_info);
      DCHECK_GT(line_box_.size(), list_marker_index.value());
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      line_box_.AddChild(
          item.GetLayoutObject(),
          box_states_->ContainingLayoutObjectForAbsolutePositionObjects(),
          item.BidiLevel());
    } else if (item.Type() == NGInlineItem::kBidiControl) {
      line_box_.AddChild(item.BidiLevel());
    }
  }

  if (line_info->LineEndShapeResult()) {
    PlaceGeneratedContent(std::move(line_info->LineEndShapeResult()),
                          std::move(line_info->LineEndStyle()), box,
                          &text_builder);
  }

  if (line_box_.IsEmpty()) {
    return;  // The line was empty.
  }

  box_states_->OnEndPlaceItems(&line_box_, baseline_type_);

  // TODO(kojii): For LTR, we can optimize ComputeInlinePositions() to compute
  // without PrepareForReorder() and UpdateAfterReorder() even when
  // HasBoxFragments(). We do this to share the logic between LTR and RTL, and
  // to get more coverage for RTL, but when we're more stabilized, we could have
  // optimized code path for LTR.
  box_states_->PrepareForReorder(&line_box_);
  BidiReorder();
  box_states_->UpdateAfterReorder(&line_box_);
  LayoutUnit inline_size = box_states_->ComputeInlinePositions(&line_box_);
  const NGLineHeightMetrics& line_box_metrics =
      box_states_->LineBoxState().metrics;

  // Handle out-of-flow positioned objects. They need inline offsets for their
  // static positions.
  if (!PlaceOutOfFlowObjects(*line_info, line_box_metrics)) {
    // If we have out-of-flow objects but nothing else, we don't have line box
    // metrics nor BFC offset. Exit early.
    return;
  }

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
  inline_size = inline_size.ClampNegativeToZero();

  // Other 'text-align' values than 'justify' move line boxes as a whole, but
  // indivisual items do not change their relative position to the line box.
  if (text_align != ETextAlign::kJustify)
    line_bfc_offset.line_offset += OffsetForTextAlign(*line_info, text_align);

  if (IsLtr(line_info->BaseDirection()))
    line_bfc_offset.line_offset += line_info->TextIndent();

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
    NGInlineBoxState* box,
    NGTextFragmentBuilder* text_builder) {
  if (box->CanAddTextOfStyle(*style)) {
    PlaceText(std::move(shape_result), std::move(style), 0, box, text_builder);
  } else {
    scoped_refptr<ComputedStyle> text_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(*style,
                                                       EDisplay::kInline);
    NGInlineBoxState* box = box_states_->OnOpenTag(*text_style, line_box_);
    box->ComputeTextMetrics(*text_style, baseline_type_, false);
    PlaceText(std::move(shape_result), std::move(style), 0, box, text_builder);
    box_states_->OnCloseTag(&line_box_, box, baseline_type_);
  }
}

void NGInlineLayoutAlgorithm::PlaceText(
    scoped_refptr<const ShapeResult> shape_result,
    scoped_refptr<const ComputedStyle> style,
    UBiDiLevel bidi_level,
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
  line_box_.AddChild(std::move(text_fragment), box->text_top, inline_size,
                     bidi_level);
}

NGInlineBoxState* NGInlineLayoutAlgorithm::PlaceAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info) {
  DCHECK(item_result->layout_result);

  // The input |position| is the line-left edge of the margin box.
  // Adjust it to the border box by adding the line-left margin.
  // const ComputedStyle& style = *item.Style();
  // position += item_result->margins.LineLeft(style.Direction());

  item_result->has_edge = true;
  NGInlineBoxState* box = box_states_->OnOpenTag(item, *item_result, line_box_);
  PlaceLayoutResult(item_result, box, box->margin_inline_start);
  return box_states_->OnCloseTag(&line_box_, box, baseline_type_);
}

// Place a NGLayoutResult into the line box.
void NGInlineLayoutAlgorithm::PlaceLayoutResult(NGInlineItemResult* item_result,
                                                NGInlineBoxState* box,
                                                LayoutUnit inline_offset) {
  DCHECK(item_result->layout_result);
  DCHECK(item_result->layout_result->PhysicalFragment());
  DCHECK(item_result->item);
  const NGInlineItem& item = *item_result->item;
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
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
    line_box_.AddChild(std::move(text_fragment), line_top, LayoutUnit(),
                       item.BidiLevel());
    // We need the box fragment as well to compute VisualRect() correctly.
  }

  line_box_.AddChild(std::move(item_result->layout_result),
                     NGLogicalOffset{inline_offset, line_top},
                     item_result->inline_size, item.BidiLevel());
}

// Place all out-of-flow objects in |line_box_| and clear them.
// @return whether |line_box_| has any in-flow fragments.
bool NGInlineLayoutAlgorithm::PlaceOutOfFlowObjects(
    const NGLineInfo& line_info,
    const NGLineHeightMetrics& line_box_metrics) {
  bool has_fragments = false;
  for (auto& child : line_box_) {
    if (LayoutObject* box = child.out_of_flow_positioned_box) {
      // The static position is at the line-top. Ignore the block_offset.
      NGLogicalOffset static_offset(child.offset.inline_offset, LayoutUnit());

      // If a block-level box appears in the middle of a line, move the static
      // position to where the next block will be placed.
      if (static_offset.inline_offset &&
          !box->StyleRef().IsOriginalDisplayInlineType()) {
        static_offset.inline_offset = LayoutUnit();
        if (!line_box_metrics.IsEmpty())
          static_offset.block_offset = line_box_metrics.LineHeight();
      }

      container_builder_.AddInlineOutOfFlowChildCandidate(
          NGBlockNode(ToLayoutBox(box)), static_offset,
          line_info.BaseDirection(), child.out_of_flow_containing_box);

      child.out_of_flow_positioned_box = child.out_of_flow_containing_box =
          nullptr;
    } else if (!has_fragments) {
      has_fragments = child.HasFragment();
    }
  }
  return has_fragments;
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
  PlaceLayoutResult(item_result, nullptr);
}

// Justify the line. This changes the size of items by adding spacing.
// Returns false if justification failed and should fall back to start-aligned.
bool NGInlineLayoutAlgorithm::ApplyJustify(NGLineInfo* line_info) {
  NGLineAlign align(*line_info);
  if (align.space <= 0)
    return false;  // no expansion is needed.

  // Construct the line text to compute spacing for.
  String line_text =
      Node().Text(line_info->StartOffset(), align.end_offset).ToString();

  // Append a hyphen if the last word is hyphenated. The hyphen is in
  // |ShapeResult|, but not in text. |ShapeResultSpacing| needs the text that
  // matches to the |ShapeResult|.
  const NGInlineItemResult& last_item_result = line_info->Results().back();
  if (last_item_result.text_end_effect == NGTextEndEffect::kHyphen)
    line_text.append(last_item_result.item->Style()->HyphenString());

  // Compute the spacing to justify.
  ShapeResultSpacing<String> spacing(line_text);
  spacing.SetExpansion(align.space, line_info->BaseDirection(),
                       line_info->LineStyle().GetTextJustify());
  if (!spacing.HasExpansion())
    return false;  // no expansion opportunities exist.

  for (NGInlineItemResult& item_result : line_info->Results()) {
    if (item_result.has_only_trailing_spaces)
      break;
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

// Compute the offset to shift the line box for the 'text-align' property.
LayoutUnit NGInlineLayoutAlgorithm::OffsetForTextAlign(
    const NGLineInfo& line_info,
    ETextAlign text_align) const {
  bool is_base_ltr = IsLtr(line_info.BaseDirection());
  while (true) {
    switch (text_align) {
      case ETextAlign::kLeft:
      case ETextAlign::kWebkitLeft: {
        // The direction of the block should determine what happens with wide
        // lines. In particular with RTL blocks, wide lines should still spill
        // out to the left.
        if (is_base_ltr)
          return LayoutUnit();
        NGLineAlign align(line_info);
        return align.space.ClampPositiveToZero();
      }
      case ETextAlign::kRight:
      case ETextAlign::kWebkitRight: {
        // Wide lines spill out of the block based off direction.
        // So even if text-align is right, if direction is LTR, wide lines
        // should overflow out of the right side of the block.
        NGLineAlign align(line_info);
        if (align.space > 0 || !is_base_ltr)
          return align.space;
        return LayoutUnit();
      }
      case ETextAlign::kCenter:
      case ETextAlign::kWebkitCenter: {
        NGLineAlign align(line_info);
        if (is_base_ltr || align.space > 0)
          return (align.space / 2).ClampNegativeToZero();
        // In RTL, wide lines should spill out to the left, same as kRight.
        return align.space;
      }
      case ETextAlign::kStart:
        text_align = is_base_ltr ? ETextAlign::kLeft : ETextAlign::kRight;
        continue;
      case ETextAlign::kEnd:
        text_align = is_base_ltr ? ETextAlign::kRight : ETextAlign::kLeft;
        continue;
      case ETextAlign::kJustify:
        // Justification is applied in earlier phase, see PlaceItems().
        NOTREACHED();
        return LayoutUnit();
    }
    NOTREACHED();
    return LayoutUnit();
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
        &descendant_candidates, nullptr);
    for (auto& descendant : descendant_candidates)
      container_builder_.AddOutOfFlowDescendant(descendant);

    return container_builder_.ToLineBoxFragment();
  }

  DCHECK(container_builder_.BfcOffset());

  // We query all the layout opportunities on the initial exclusion space up
  // front, as if the line breaker may add floats and change the opportunities.
  Vector<NGLayoutOpportunity> opportunities =
      initial_exclusion_space->AllLayoutOpportunities(
          ConstraintSpace().BfcOffset(),
          ConstraintSpace().AvailableSize().inline_size);

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
    NGLineBreaker line_breaker(Node(), NGLineBreakerMode::kContent,
                               constraint_space_, &positioned_floats,
                               &unpositioned_floats_, exclusion_space.get(),
                               handled_item_index, break_token);

    // TODO(ikilpatrick): Does this always succeed when we aren't an empty
    // inline?
    if (!line_breaker.NextLine(opportunity, &line_info))
      break;

    // If this fragment will be larger than the inline-size of the opportunity,
    // *and* the opportunity is smaller than the available inline-size,
    // continue to the next opportunity.
    if (line_info.Width() > opportunity.rect.InlineSize() &&
        opportunity.rect.InlineSize() !=
            ConstraintSpace().AvailableSize().inline_size)
      continue;

    CreateLine(&line_info, exclusion_space.get());

    // We now can check the block-size of the fragment, and it fits within the
    // opportunity.
    LayoutUnit block_size = container_builder_.ComputeBlockSize();
    if (block_size > opportunity.rect.BlockSize())
      continue;

    if (opportunity.rect.BlockStartOffset() >
        ConstraintSpace().BfcOffset().block_offset)
      container_builder_.SetIsPushedByFloats();

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
      &descendant_candidates, nullptr);
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
          node, NGLogicalOffset(), Style().Direction(), nullptr);
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

  if (BreakToken() && BreakToken()->IgnoreFloats()) {
    unpositioned_floats_.clear();
    return;
  }

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

void NGInlineLayoutAlgorithm::BidiReorder() {
  // TODO(kojii): UAX#9 L1 is not supported yet. Supporting L1 may change
  // embedding levels of parts of runs, which requires to split items.
  // http://unicode.org/reports/tr9/#L1
  // BidiResolver does not support L1 crbug.com/316409.

  // Create a list of chunk indices in the visual order.
  // ICU |ubidi_getVisualMap()| works for a run of characters. Since we can
  // handle the direction of each run, we use |ubidi_reorderVisual()| to reorder
  // runs instead of characters.
  NGLineBoxFragmentBuilder::ChildList logical_items;
  Vector<UBiDiLevel, 32> levels;
  logical_items.ReserveInitialCapacity(line_box_.size());
  levels.ReserveInitialCapacity(line_box_.size());
  for (auto& item : line_box_) {
    if (!item.HasFragment() && !item.HasBidiLevel())
      continue;
    levels.push_back(item.bidi_level);
    logical_items.AddChild(std::move(item));
    DCHECK(!item.HasInFlowFragment());
  }

  Vector<int32_t, 32> indices_in_visual_order(levels.size());
  NGBidiParagraph::IndicesInVisualOrder(levels, &indices_in_visual_order);

  // Reorder to the visual order.
  line_box_.resize(0);
  for (unsigned logical_index : indices_in_visual_order) {
    line_box_.AddChild(std::move(logical_items[logical_index]));
    DCHECK(!logical_items[logical_index].HasInFlowFragment());
  }
}

}  // namespace blink
