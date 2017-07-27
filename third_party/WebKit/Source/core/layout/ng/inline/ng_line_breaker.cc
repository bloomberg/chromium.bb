// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_line_breaker.h"

#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/ShapingLineBreaker.h"

namespace blink {

namespace {

// Use a mock of ShapingLineBreaker for test/debug purposes.
// #define MOCK_SHAPE_LINE

#if defined(MOCK_SHAPE_LINE)
// The mock for ShapingLineBreaker::ShapeLine().
// See BreakText() for the expected semantics.
std::pair<unsigned, LayoutUnit> ShapeLineMock(
    const NGInlineItem& item,
    unsigned offset,
    LayoutUnit available_width,
    const LazyLineBreakIterator& break_iterator) {
  bool has_break_opportunities = false;
  LayoutUnit inline_size;
  while (true) {
    unsigned next_break = break_iterator.NextBreakOpportunity(offset + 1);
    next_break = std::min(next_break, item.EndOffset());
    LayoutUnit next_inline_size =
        inline_size + item.InlineSize(offset, next_break);
    if (next_inline_size > available_width) {
      if (!has_break_opportunities)
        return std::make_pair(next_break, next_inline_size);
      return std::make_pair(offset, inline_size);
    }
    if (next_break >= item.EndOffset())
      return std::make_pair(next_break, next_inline_size);
    offset = next_break;
    inline_size = next_inline_size;
    has_break_opportunities = true;
  }
}
#endif

}  // namespace

NGLineBreaker::NGLineBreaker(
    NGInlineNode node,
    NGConstraintSpace* space,
    NGFragmentBuilder* container_builder,
    Vector<RefPtr<NGUnpositionedFloat>>* unpositioned_floats,
    const NGInlineBreakToken* break_token)
    : node_(node),
      constraint_space_(space),
      container_builder_(container_builder),
      unpositioned_floats_(unpositioned_floats),
      break_iterator_(node.Text()),
      shaper_(node.Text().Characters16(), node.Text().length()),
      spacing_(node.Text()) {
  if (break_token) {
    item_index_ = break_token->ItemIndex();
    offset_ = break_token->TextOffset();
    node.AssertOffset(item_index_, offset_);
  }
}

// @return if this is the "first formatted line".
// https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
bool NGLineBreaker::IsFirstFormattedLine() const {
  if (item_index_ || offset_)
    return false;
  // The first line of an anonymous block box is only affected if it is the
  // first child of its parent element.
  // https://drafts.csswg.org/css-text-3/#text-indent-property
  LayoutBlockFlow* block = node_.GetLayoutBlockFlow();
  if (block->IsAnonymousBlock() && block->PreviousSibling()) {
    // TODO(kojii): In NG, leading OOF creates a block box.
    // text-indent-first-line-002.html fails for this reason.
    // crbug.com/734554
    return false;
  }
  return true;
}

// Initialize internal states for the next line.
void NGLineBreaker::PrepareNextLine(NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  item_results->clear();
  line_info->SetLineStyle(node_, *constraint_space_, IsFirstFormattedLine(),
                          line_.is_after_forced_break);
  SetCurrentStyle(line_info->LineStyle());
  line_.is_after_forced_break = false;
  line_.should_create_line_box = false;

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  line_.position = line_info->TextIndent();

  // We are only able to calculate our available_width if our container has
  // been positioned in the BFC coordinate space yet.
  if (container_builder_->BfcOffset())
    UpdateAvailableWidth();
  else
    line_.opportunity.reset();
}

bool NGLineBreaker::NextLine(NGLineInfo* line_info,
                             const NGLogicalOffset& content_offset) {
  content_offset_ = content_offset;

  PrepareNextLine(line_info);

  BreakLine(line_info);

  // TODO(kojii): When editing, or caret is enabled, trailing spaces at wrap
  // point should not be removed. For other cases, we can a) remove, b) leave
  // characters without glyphs, or c) leave both characters and glyphs without
  // measuring. Need to decide which one works the best.
  SkipCollapsibleWhitespaces();

  if (line_info->Results().IsEmpty())
    return false;

  // TODO(kojii): There are cases where we need to PlaceItems() without creating
  // line boxes. These cases need to be reviewed.
  if (line_.should_create_line_box)
    ComputeLineLocation(line_info);

  return true;
}

void NGLineBreaker::BreakLine(NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  const Vector<NGInlineItem>& items = node_.Items();
  LineBreakState state = LineBreakState::kNotBreakable;

  while (item_index_ < items.size()) {
#if DCHECK_IS_ON()
    if (!item_results->IsEmpty() && (state == LineBreakState::kIsBreakable ||
                                     state == LineBreakState::kNotBreakable)) {
      DCHECK_EQ(item_results->back().prohibit_break_after,
                state == LineBreakState::kNotBreakable);
    }
#endif

    // CloseTag prohibits to break before.
    const NGInlineItem& item = items[item_index_];
    if (item.Type() == NGInlineItem::kCloseTag) {
      state = HandleCloseTag(item, item_results);
      continue;
    }

    if (state == LineBreakState::kBreakAfterTrailings) {
      line_info->SetIsLastLine(false);
      return;
    }
    if (state == LineBreakState::kIsBreakable && line_.HasAvailableWidth() &&
        !line_.CanFit())
      return HandleOverflow(line_info);

    item_results->push_back(
        NGInlineItemResult(item_index_, offset_, item.EndOffset()));
    NGInlineItemResult* item_result = &item_results->back();
    if (item.Type() == NGInlineItem::kText) {
      state = HandleText(*item_results, item, item_result);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      state = HandleAtomicInline(item, item_result, *line_info);
    } else if (item.Type() == NGInlineItem::kControl) {
      state = HandleControlItem(item, item_result);
      if (state == LineBreakState::kForcedBreak) {
        line_.is_after_forced_break = true;
        line_info->SetIsLastLine(true);
        return;
      }
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      HandleOpenTag(item, item_result);
      state = LineBreakState::kNotBreakable;
    } else if (item.Type() == NGInlineItem::kFloating) {
      state = HandleFloat(item, item_result);
    } else {
      MoveToNextOf(item);
      item_result->prohibit_break_after = true;
      state = LineBreakState::kNotBreakable;
    }
  }
  if (state == LineBreakState::kIsBreakable && line_.HasAvailableWidth() &&
      !line_.CanFit())
    return HandleOverflow(line_info);
  line_info->SetIsLastLine(true);
}

// Update the inline size of the first layout opportunity from the given
// content_offset.
void NGLineBreaker::UpdateAvailableWidth() {
  NGLogicalOffset offset = container_builder_->BfcOffset().value();
  offset += content_offset_;

  NGLayoutOpportunityIterator iter(constraint_space_->Exclusions().get(),
                                   constraint_space_->AvailableSize(), offset);
  line_.opportunity = iter.Next();
}

void NGLineBreaker::ComputeLineLocation(NGLineInfo* line_info) const {
  // Both NGLayoutOpportunity and BfcOffset are in visual order that
  // "inline-start" are actually "line-left".
  // https://drafts.csswg.org/css-writing-modes-3/#line-left
  LayoutUnit line_left = line_.opportunity.value().InlineStartOffset() -
                         constraint_space_->BfcOffset().inline_offset;
  line_info->SetLineLocation(line_left, line_.AvailableWidth(),
                             content_offset_.block_offset);
}

bool NGLineBreaker::IsFirstBreakOpportunity(
    unsigned offset,
    const NGInlineItemResults& results) const {
  unsigned line_start_offset = results.front().start_offset;
  return break_iterator_.NextBreakOpportunity(line_start_offset) >= offset;
}

NGLineBreaker::LineBreakState NGLineBreaker::ComputeIsBreakableAfter(
    NGInlineItemResult* item_result) const {
  if (auto_wrap_ && break_iterator_.IsBreakable(item_result->end_offset)) {
    DCHECK(!item_result->prohibit_break_after);
    return LineBreakState::kIsBreakable;
  }
  item_result->prohibit_break_after = true;
  return LineBreakState::kNotBreakable;
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleText(
    const NGInlineItemResults& results,
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  line_.should_create_line_box = true;

  LayoutUnit available_width = line_.AvailableWidth();

  // If the start offset is at the item boundary, try to add the entire item.
  if (offset_ == item.StartOffset()) {
    item_result->inline_size = item.InlineSize();
    LayoutUnit next_position = line_.position + item_result->inline_size;
    if (!auto_wrap_ || next_position <= available_width) {
      item_result->shape_result = item.TextShapeResult();
      item_result->no_break_opportunities_inside = !auto_wrap_;
      line_.position = next_position;
      MoveToNextOf(item);
      return ComputeIsBreakableAfter(item_result);
    }
  }

  if (auto_wrap_) {
    // Try to break inside of this text item.
    BreakText(item_result, item, available_width - line_.position);
    LayoutUnit next_position = line_.position + item_result->inline_size;
    bool is_overflow = next_position > available_width;

    // If overflow and no break opportunities exist, and if 'break-word', try to
    // break at every grapheme cluster boundary.
    if (is_overflow && break_if_overflow_ &&
        IsFirstBreakOpportunity(item_result->end_offset, results)) {
      DCHECK_EQ(break_iterator_.BreakType(), LineBreakType::kNormal);
      break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
      BreakText(item_result, item, available_width - line_.position);
      break_iterator_.SetBreakType(LineBreakType::kNormal);
      next_position = line_.position + item_result->inline_size;
      is_overflow = next_position > available_width;
    }

    line_.position = next_position;
    item_result->no_break_opportunities_inside = is_overflow;
    if (item_result->end_offset < item.EndOffset()) {
      offset_ = item_result->end_offset;
      return is_overflow ? LineBreakState::kIsBreakable
                         : LineBreakState::kBreakAfterTrailings;
    }
    MoveToNextOf(item);
    return item_result->prohibit_break_after ? LineBreakState::kNotBreakable
                                             : LineBreakState::kIsBreakable;
  }

  // Add the rest of the item if !auto_wrap.
  // Because the start position may need to reshape, run ShapingLineBreaker
  // with max available width.
  DCHECK_NE(offset_, item.StartOffset());
  BreakText(item_result, item, LayoutUnit::Max());
  DCHECK_EQ(item_result->end_offset, item.EndOffset());
  item_result->no_break_opportunities_inside = true;
  item_result->prohibit_break_after = true;
  line_.position += item_result->inline_size;
  MoveToNextOf(item);
  return LineBreakState::kNotBreakable;
}

void NGLineBreaker::BreakText(NGInlineItemResult* item_result,
                              const NGInlineItem& item,
                              LayoutUnit available_width) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  item.AssertOffset(item_result->start_offset);

#if defined(MOCK_SHAPE_LINE)
  std::tie(item_result->end_offset, item_result->inline_size) = ShapeLineMock(
      item, item_result->start_offset, available_width, break_iterator_);
#else
  // TODO(kojii): We need to instantiate ShapingLineBreaker here because it
  // has item-specific info as context. Should they be part of ShapeLine() to
  // instantiate once, or is this just fine since instatiation is not
  // expensive?
  DCHECK_EQ(item.TextShapeResult()->StartIndexForResult(), item.StartOffset());
  DCHECK_EQ(item.TextShapeResult()->EndIndexForResult(), item.EndOffset());
  ShapingLineBreaker breaker(&shaper_, &item.Style()->GetFont(),
                             item.TextShapeResult(), &break_iterator_,
                             &spacing_);
  available_width = std::max(LayoutUnit(0), available_width);
  item_result->shape_result = breaker.ShapeLine(
      item_result->start_offset, available_width, &item_result->end_offset);
  item_result->inline_size = item_result->shape_result->SnappedWidth();
#endif
  DCHECK_GT(item_result->end_offset, item_result->start_offset);
  // * If width <= available_width:
  //   * If offset < item.EndOffset(): the break opportunity to fit is found.
  //   * If offset == item.EndOffset(): the break opportunity at the end fits,
  //     or the first break opportunity is beyond the end.
  //     There may be room for more characters.
  // * If width > available_width: The first break opporunity does not fit.
  //   offset is the first break opportunity, either inside, at the end, or
  //   beyond the end.
  if (item_result->end_offset < item.EndOffset()) {
    item_result->prohibit_break_after = false;
  } else {
    DCHECK_EQ(item_result->end_offset, item.EndOffset());
    item_result->prohibit_break_after =
        !break_iterator_.IsBreakable(item_result->end_offset);
  }
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
NGLineBreaker::LineBreakState NGLineBreaker::HandleControlItem(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Length(), 1u);
  line_.should_create_line_box = true;

  UChar character = node_.Text()[item.StartOffset()];
  if (character == kNewlineCharacter) {
    MoveToNextOf(item);
    return LineBreakState::kForcedBreak;
  }
  DCHECK_EQ(character, kTabulationCharacter);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  const Font& font = style.GetFont();
  item_result->inline_size = font.TabWidth(style.GetTabSize(), line_.position);
  line_.position += item_result->inline_size;
  MoveToNextOf(item);
  // TODO(kojii): Implement break around the tab character.
  return LineBreakState::kIsBreakable;
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  line_.should_create_line_box = true;

  LayoutBox* layout_box = ToLayoutBox(item.GetLayoutObject());
  NGBlockNode node = NGBlockNode(layout_box);
  const ComputedStyle& style = node.Style();
  NGConstraintSpaceBuilder constraint_space_builder(constraint_space_);
  // Request to compute baseline during the layout, except when we know the box
  // would synthesize box-baseline.
  if (NGBaseline::ShouldPropagateBaselines(layout_box)) {
    constraint_space_builder.AddBaselineRequest(
        {line_info.UseFirstLineStyle()
             ? NGBaselineAlgorithmType::kAtomicInlineForFirstLine
             : NGBaselineAlgorithmType::kAtomicInline,
         IsHorizontalWritingMode(constraint_space_->WritingMode())
             ? FontBaseline::kAlphabeticBaseline
             : FontBaseline::kIdeographicBaseline});
  }
  RefPtr<NGConstraintSpace> constraint_space =
      constraint_space_builder.SetIsNewFormattingContext(true)
          .SetIsShrinkToFit(true)
          .SetTextDirection(style.Direction())
          .ToConstraintSpace(FromPlatformWritingMode(style.GetWritingMode()));
  item_result->layout_result = node.Layout(constraint_space.Get());

  item_result->inline_size =
      NGBoxFragment(constraint_space_->WritingMode(),
                    ToNGPhysicalBoxFragment(
                        item_result->layout_result->PhysicalFragment().Get()))
          .InlineSize();

  item_result->margins =
      ComputeMargins(*constraint_space_, style,
                     constraint_space_->WritingMode(), style.Direction());
  item_result->inline_size += item_result->margins.InlineSum();

  line_.position += item_result->inline_size;
  MoveToNextOf(item);
  return ComputeIsBreakableAfter(item_result);
}

// Performs layout and positions a float.
//
// If there is a known available_width (e.g. something has resolved the
// container BFC offset) it will attempt to position the float on the current
// line.
// Additionally updates the available_width for the line as the float has
// (probably) consumed space.
//
// If the float is too wide *or* we already have UnpositionedFloats we add it
// as an UnpositionedFloat. This should be positioned *immediately* after we
// are done with the current line.
// We have this check if there are already UnpositionedFloats as we aren't
// allowed to position a float "above" another float which has come before us
// in the document.
//
// TODO(glebl): Add the support of clearance for inline floats.
NGLineBreaker::LineBreakState NGLineBreaker::HandleFloat(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));

  const ComputedStyle& float_style = node.Style();
  NGBoxStrut margins = ComputeMargins(*constraint_space_, float_style,
                                      constraint_space_->WritingMode(),
                                      constraint_space_->Direction());

  // TODO(ikilpatrick): Add support for float break tokens inside an inline
  // layout context.
  RefPtr<NGUnpositionedFloat> unpositioned_float = NGUnpositionedFloat::Create(
      constraint_space_->AvailableSize(),
      constraint_space_->PercentageResolutionSize(),
      constraint_space_->BfcOffset().inline_offset,
      constraint_space_->BfcOffset().inline_offset, margins, node,
      /* break_token */ nullptr);

  LayoutUnit inline_size = ComputeInlineSizeForUnpositionedFloat(
      constraint_space_, unpositioned_float.Get());

  // We can only determine if our float will fit if we have an available_width
  // I.e. we may not have come across any text yet, in order to be able to
  // resolve the BFC position.
  bool float_does_not_fit = (!constraint_space_->FloatsBfcOffset() ||
                             container_builder_->BfcOffset()) &&
                            (!line_.HasAvailableWidth() ||
                             !line_.CanFit(inline_size + margins.InlineSum()));

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!unpositioned_floats_->IsEmpty() || float_does_not_fit) {
    unpositioned_floats_->push_back(std::move(unpositioned_float));
  } else {
    NGLogicalOffset container_bfc_offset =
        container_builder_->BfcOffset()
            ? container_builder_->BfcOffset().value()
            : constraint_space_->FloatsBfcOffset().value();
    LayoutUnit origin_block_offset =
        container_bfc_offset.block_offset + content_offset_.block_offset;

    NGPositionedFloat positioned_float =
        PositionFloat(origin_block_offset, container_bfc_offset.block_offset,
                      unpositioned_float.Get(), constraint_space_);
    container_builder_->AddChild(positioned_float.layout_result,
                                 positioned_float.logical_offset);

    // We need to recalculate the available_width as the float probably
    // consumed space on the line.
    if (container_builder_->BfcOffset())
      UpdateAvailableWidth();
  }

  MoveToNextOf(item);
  return ComputeIsBreakableAfter(item_result);
}

void NGLineBreaker::HandleOpenTag(const NGInlineItem& item,
                                  NGInlineItemResult* item_result) {
  item_result->prohibit_break_after = true;

  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  item_result->needs_box_when_empty = false;
  if (style.HasBorder() || style.HasPadding() ||
      (style.HasMargin() && item.HasStartEdge())) {
    NGBoxStrut borders = ComputeBorders(*constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(*constraint_space_, style);
    item_result->borders_paddings_block_start =
        borders.block_start + paddings.block_start;
    item_result->borders_paddings_block_end =
        borders.block_end + paddings.block_end;
    if (item.HasStartEdge()) {
      item_result->margins = ComputeMargins(*constraint_space_, style,
                                            constraint_space_->WritingMode(),
                                            constraint_space_->Direction());
      item_result->inline_size = item_result->margins.inline_start +
                                 borders.inline_start + paddings.inline_start;
      line_.position += item_result->inline_size;

      // While the spec defines "non-zero margins, padding, or borders" prevents
      // line boxes to be zero-height, tests indicate that only inline direction
      // of them do so. See should_create_line_box_.
      // Force to create a box, because such inline boxes affect line heights.
      item_result->needs_box_when_empty =
          item_result->inline_size || item_result->margins.inline_start;
      line_.should_create_line_box |= item_result->needs_box_when_empty;
    }
  }
  SetCurrentStyle(style);
  MoveToNextOf(item);
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleCloseTag(
    const NGInlineItem& item,
    NGInlineItemResults* item_results) {
  item_results->push_back(
      NGInlineItemResult(item_index_, offset_, item.EndOffset()));
  NGInlineItemResult* item_result = &item_results->back();

  item_result->needs_box_when_empty = false;
  if (item.HasEndEdge()) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    item_result->margins = ComputeMargins(*constraint_space_, style,
                                          constraint_space_->WritingMode(),
                                          constraint_space_->Direction());
    NGBoxStrut borders = ComputeBorders(*constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(*constraint_space_, style);
    item_result->inline_size = item_result->margins.inline_end +
                               borders.inline_end + paddings.inline_end;
    line_.position += item_result->inline_size;

    item_result->needs_box_when_empty =
        item_result->inline_size || item_result->margins.inline_end;
    line_.should_create_line_box |= item_result->needs_box_when_empty;
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);

  // Prohibit break before a close tag by setting prohibit_break_after to the
  // previous result.
  // TODO(kojii): There should be a result before close tag, but there are cases
  // that doesn't because of the way we handle trailing spaces. This needs to be
  // revisited.
  if (item_results->size() >= 2) {
    NGInlineItemResult* last = &(*item_results)[item_results->size() - 2];
    if (was_auto_wrap == auto_wrap_) {
      item_result->prohibit_break_after = last->prohibit_break_after;
      last->prohibit_break_after = true;
      return item_result->prohibit_break_after ? LineBreakState::kNotBreakable
                                               : LineBreakState::kIsBreakable;
    }
    last->prohibit_break_after = true;
  }
  return ComputeIsBreakableAfter(item_result);
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
void NGLineBreaker::HandleOverflow(NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  const Vector<NGInlineItem>& items = node_.Items();
  LayoutUnit available_width = line_.AvailableWidth();
  LayoutUnit width_to_rewind = line_.position - available_width;
  DCHECK_GT(width_to_rewind, 0);

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results)[--i];

    // Try to break after this item.
    if (i < item_results->size() - 1 && !item_result->prohibit_break_after) {
      if (width_to_rewind <= 0)
        return Rewind(line_info, i + 1);
      break_before = i + 1;
    }

    // Try to break inside of this item.
    LayoutUnit next_width_to_rewind =
        width_to_rewind - item_result->inline_size;
    const NGInlineItem& item = items[item_result->item_index];
    if (item.Type() == NGInlineItem::kText && next_width_to_rewind < 0 &&
        !item_result->no_break_opportunities_inside) {
      // When the text fits but its right margin does not, the break point
      // must not be at the end.
      LayoutUnit item_available_width =
          std::min(-next_width_to_rewind, item_result->inline_size - 1);
      BreakText(item_result, item, item_available_width);
      if (item_result->inline_size <= item_available_width) {
        DCHECK_LT(item_result->end_offset, item.EndOffset());
        DCHECK(!item_result->prohibit_break_after);
        return Rewind(line_info, i + 1);
      }
    }

    width_to_rewind = next_width_to_rewind;
  }

  // The rewind point did not found, let this line overflow.
  // If there was a break opporunity, the overflow should stop there.
  if (break_before)
    return Rewind(line_info, break_before);

  line_info->SetIsLastLine(item_index_ >= items.size());
}

void NGLineBreaker::Rewind(NGLineInfo* line_info, unsigned new_end) {
  NGInlineItemResults* item_results = &line_info->Results();
  // TODO(kojii): Should we keep results for the next line? We don't need to
  // re-layout atomic inlines.
  // TODO(kojii): Removing processed floats is likely a problematic. Keep
  // floats in this line, or keep it for the next line.
  item_results->Shrink(new_end);

  MoveToNextOf(item_results->back());
  DCHECK_LT(item_index_, node_.Items().size());
  line_info->SetIsLastLine(false);
}

void NGLineBreaker::SetCurrentStyle(const ComputedStyle& style) {
  auto_wrap_ = style.AutoWrap();

  if (auto_wrap_) {
    break_iterator_.SetLocale(style.LocaleForLineBreakIterator());

    switch (style.WordBreak()) {
      case EWordBreak::kNormal:
        break_if_overflow_ = style.OverflowWrap() == EOverflowWrap::kBreakWord;
        break_iterator_.SetBreakType(LineBreakType::kNormal);
        break;
      case EWordBreak::kBreakAll:
        break_if_overflow_ = false;
        break_iterator_.SetBreakType(LineBreakType::kBreakAll);
        break;
      case EWordBreak::kBreakWord:
        break_if_overflow_ = true;
        break_iterator_.SetBreakType(LineBreakType::kNormal);
        break;
      case EWordBreak::kKeepAll:
        break_if_overflow_ = false;
        break_iterator_.SetBreakType(LineBreakType::kKeepAll);
        break;
    }
    break_iterator_.SetBreakAfterSpace(style.BreakOnlyAfterWhiteSpace());
  }

  spacing_.SetSpacing(style.GetFontDescription());
}

void NGLineBreaker::MoveToNextOf(const NGInlineItem& item) {
  DCHECK_EQ(&item, &node_.Items()[item_index_]);
  offset_ = item.EndOffset();
  item_index_++;
}

void NGLineBreaker::MoveToNextOf(const NGInlineItemResult& item_result) {
  offset_ = item_result.end_offset;
  item_index_ = item_result.item_index;
  const NGInlineItem& item = node_.Items()[item_result.item_index];
  if (offset_ == item.EndOffset())
    item_index_++;
}

void NGLineBreaker::SkipCollapsibleWhitespaces() {
  const Vector<NGInlineItem>& items = node_.Items();
  if (item_index_ >= items.size())
    return;
  const NGInlineItem& item = items[item_index_];
  if (item.Type() != NGInlineItem::kText || !item.Style()->CollapseWhiteSpace())
    return;

  DCHECK_LT(offset_, item.EndOffset());
  if (node_.Text()[offset_] == kSpaceCharacter) {
    // Skip one whitespace. Collapsible spaces are collapsed to single space in
    // NGInlineItemBuilder, so this removes all collapsible spaces.
    offset_++;
    if (offset_ == item.EndOffset())
      item_index_++;
  }
}

RefPtr<NGInlineBreakToken> NGLineBreaker::CreateBreakToken() const {
  const Vector<NGInlineItem>& items = node_.Items();
  if (item_index_ >= items.size())
    return nullptr;
  return NGInlineBreakToken::Create(node_, item_index_, offset_);
}

}  // namespace blink
