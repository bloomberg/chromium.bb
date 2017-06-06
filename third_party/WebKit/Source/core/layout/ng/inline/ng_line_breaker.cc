// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_line_breaker.h"

#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_layout_algorithm.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_text_fragment.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapingLineBreaker.h"

namespace blink {

namespace {

// Use a mock of ShapingLineBreaker for test/debug purposes.
#define MOCK_SHAPE_LINE

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
    LayoutUnit next_inline_size =
        inline_size +
        item.InlineSize(offset, std::min(next_break, item.EndOffset()));
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

NGLineBreaker::NGLineBreaker(NGInlineNode node,
                             const NGConstraintSpace* space,
                             NGInlineBreakToken* break_token)
    : node_(node),
      constraint_space_(space),
      item_index_(0),
      offset_(0),
      break_iterator_(node.Text()) {
  if (break_token) {
    item_index_ = break_token->ItemIndex();
    offset_ = break_token->TextOffset();
    node.AssertOffset(item_index_, offset_);
  }
}

void NGLineBreaker::NextLine(NGInlineItemResults* item_results,
                             NGInlineLayoutAlgorithm* algorithm) {
  BreakLine(item_results, algorithm);

  // TODO(kojii): When editing, or caret is enabled, trailing spaces at wrap
  // point should not be removed. For other cases, we can a) remove, b) leave
  // characters without glyphs, or c) leave both characters and glyphs without
  // measuring. Need to decide which one works the best.
  SkipCollapsibleWhitespaces();
}

void NGLineBreaker::BreakLine(NGInlineItemResults* item_results,
                              NGInlineLayoutAlgorithm* algorithm) {
  DCHECK(item_results->IsEmpty());
  const Vector<NGInlineItem>& items = node_.Items();
  const ComputedStyle& style = node_.Style();
  UpdateBreakIterator(style);
#if !defined(MOCK_SHAPE_LINE)
  // TODO(kojii): Instantiate in the constructor.
  HarfBuzzShaper shaper(text.Characters16(), text.length());
#endif
  available_width_ = algorithm->AvailableWidth();
  position_ = LayoutUnit(0);
  LineBreakState state = LineBreakState::kNotBreakable;

  while (item_index_ < items.size()) {
    // CloseTag prohibits to break before.
    const NGInlineItem& item = items[item_index_];
    if (item.Type() == NGInlineItem::kCloseTag) {
      item_results->push_back(
          NGInlineItemResult(item_index_, offset_, item.EndOffset()));
      HandleCloseTag(item, &item_results->back());
      continue;
    }

    if (state == LineBreakState::kBreakAfterTrailings)
      return;
    if (state == LineBreakState::kIsBreakable && position_ > available_width_)
      return HandleOverflow(item_results);

    item_results->push_back(
        NGInlineItemResult(item_index_, offset_, item.EndOffset()));
    NGInlineItemResult* item_result = &item_results->back();
    if (item.Type() == NGInlineItem::kText) {
      state = HandleText(item, item_result);
    } else if (item.Type() == NGInlineItem::kAtomicInline) {
      state = HandleAtomicInline(item, item_result);
    } else if (item.Type() == NGInlineItem::kControl) {
      state = HandleControlItem(item, item_result);
      if (state == LineBreakState::kForcedBreak)
        return;
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      HandleOpenTag(item, item_result);
      state = LineBreakState::kNotBreakable;
    } else if (item.Type() == NGInlineItem::kFloating) {
      HandleFloat(item, item_results, algorithm);
    } else {
      MoveToNextOf(item);
    }
  }
  if (state == LineBreakState::kIsBreakable && position_ > available_width_)
    return HandleOverflow(item_results);
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleText(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);

  // If the start offset is at the item boundary, try to add the entire item.
  if (offset_ == item.StartOffset()) {
    item_result->inline_size = item.InlineSize();
    LayoutUnit next_position = position_ + item_result->inline_size;
    if (!auto_wrap_ || next_position <= available_width_) {
      position_ = next_position;
      MoveToNextOf(item);
      if (auto_wrap_ && break_iterator_.IsBreakable(item.EndOffset()))
        return LineBreakState::kIsBreakable;
      item_result->prohibit_break_after = true;
      return LineBreakState::kNotBreakable;
    }
  }

  if (auto_wrap_) {
    // Try to break inside of this text item.
    BreakText(item_result, item, available_width_ - position_);
    position_ += item_result->inline_size;

    bool is_overflow = position_ > available_width_;
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
  position_ += item_result->inline_size;
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
  ShapingLineBreaker breaker(&shaper, &item.Style()->GetFont(),
                             item.TextShapeResult(), break_iterator_);
  item_result->shape_result = breaker.ShapeLine(
      item_result->start_offset, available_width, &item_result->end_offset);
  item_result->inline_size = item_result->shape_result->SnappedWidth();
#endif
  DCHECK_GT(item_result->end_offset, item_result->start_offset);
  // * If width <= available_width:
  //   * If offset < item.EndOffset(): the break opportunity to fit is found.
  //   * If offset == item.EndOffset(): the break opportunity at the end fits.
  //     There may be room for more characters.
  //   * If offset > item.EndOffset(): the first break opportunity is beyond
  //     the end. There may be room for more characters.
  // * If width > available_width: The first break opporunity does not fit.
  //   offset is the first break opportunity, either inside, at the end, or
  //   beyond the end.
  if (item_result->end_offset <= item.EndOffset()) {
    item_result->prohibit_break_after = false;
  } else {
    item_result->prohibit_break_after = true;
    item_result->end_offset = item.EndOffset();
  }
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
NGLineBreaker::LineBreakState NGLineBreaker::HandleControlItem(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Length(), 1u);
  UChar character = node_.Text()[item.StartOffset()];
  if (character == kNewlineCharacter) {
    MoveToNextOf(item);
    return LineBreakState::kForcedBreak;
  }
  DCHECK_EQ(character, kTabulationCharacter);
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  const Font& font = style.GetFont();
  item_result->inline_size = font.TabWidth(style.GetTabSize(), position_);
  position_ += item_result->inline_size;
  MoveToNextOf(item);
  // TODO(kojii): Implement break around the tab character.
  return LineBreakState::kIsBreakable;
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  NGBlockNode node = NGBlockNode(ToLayoutBox(item.GetLayoutObject()));
  const ComputedStyle& style = node.Style();
  NGConstraintSpaceBuilder constraint_space_builder(constraint_space_);
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

  position_ += item_result->inline_size;
  MoveToNextOf(item);
  if (auto_wrap_)
    return LineBreakState::kIsBreakable;
  item_result->prohibit_break_after = true;
  return LineBreakState::kNotBreakable;
}

void NGLineBreaker::HandleFloat(const NGInlineItem& item,
                                NGInlineItemResults* item_results,
                                NGInlineLayoutAlgorithm* algorithm) {
  algorithm->LayoutAndPositionFloat(position_, item.GetLayoutObject());
  // Floats may change the available width if they fit.
  available_width_ = algorithm->AvailableWidth();
  // Floats are already positioned in the container_builder.
  item_results->pop_back();
  MoveToNextOf(item);
}

void NGLineBreaker::HandleOpenTag(const NGInlineItem& item,
                                  NGInlineItemResult* item_result) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
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
      position_ += item_result->inline_size;
    }
  }
  UpdateBreakIterator(style);
  MoveToNextOf(item);
}

void NGLineBreaker::HandleCloseTag(const NGInlineItem& item,
                                   NGInlineItemResult* item_result) {
  if (item.HasEndEdge()) {
    DCHECK(item.Style());
    item_result->margins = ComputeMargins(*constraint_space_, *item.Style(),
                                          constraint_space_->WritingMode(),
                                          constraint_space_->Direction());
    NGBoxStrut borders = ComputeBorders(*constraint_space_, *item.Style());
    NGBoxStrut paddings = ComputePadding(*constraint_space_, *item.Style());
    item_result->inline_size = item_result->margins.inline_end +
                               borders.inline_end + paddings.inline_end;
    position_ += item_result->inline_size;
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  UpdateBreakIterator(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
void NGLineBreaker::HandleOverflow(NGInlineItemResults* item_results) {
  const Vector<NGInlineItem>& items = node_.Items();
  LayoutUnit rewind_width = available_width_ - position_;
  DCHECK_LT(rewind_width, 0);

  // Search for a break opportunity that can fit.
  // Also keep track of the first break opportunity in case of overflow.
  unsigned break_before = 0;
  unsigned break_before_if_before_allow = 0;
  LayoutUnit rewind_width_if_before_allow;
  bool last_item_prohibits_break_before = true;
  for (unsigned i = item_results->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results)[--i];
    const NGInlineItem& item = items[item_result->item_index];
    rewind_width += item_result->inline_size;
    if (item.Type() == NGInlineItem::kText ||
        item.Type() == NGInlineItem::kAtomicInline) {
      // Try to break inside of this item.
      if (item.Type() == NGInlineItem::kText && rewind_width >= 0 &&
          !item_result->no_break_opportunities_inside) {
        // When the text fits but its right margin does not, the break point
        // must not be at the end.
        LayoutUnit item_available_width =
            std::min(rewind_width, item_result->inline_size - 1);
        BreakText(item_result, item, item_available_width);
        if (item_result->inline_size <= item_available_width) {
          DCHECK_LT(item_result->end_offset, item.EndOffset());
          DCHECK(!item_result->prohibit_break_after);
          return Rewind(item_results, i + 1);
        }
        if (!item_result->prohibit_break_after &&
            !last_item_prohibits_break_before) {
          break_before = i + 1;
        }
      }

      // Try to break after this item.
      if (break_before_if_before_allow && !item_result->prohibit_break_after) {
        if (rewind_width_if_before_allow >= 0)
          return Rewind(item_results, break_before_if_before_allow);
        break_before = break_before_if_before_allow;
      }

      // Otherwise, before this item is a possible break point.
      break_before_if_before_allow = i;
      rewind_width_if_before_allow = rewind_width;
      last_item_prohibits_break_before = false;
    } else if (item.Type() == NGInlineItem::kCloseTag) {
      last_item_prohibits_break_before = true;
    } else {
      if (i + 1 == break_before_if_before_allow) {
        break_before_if_before_allow = i;
        rewind_width_if_before_allow = rewind_width;
      }
      last_item_prohibits_break_before = false;
    }
  }

  // The rewind point did not found, let this line overflow.
  // If there was a break opporunity, the overflow should stop there.
  if (break_before)
    Rewind(item_results, break_before);
}

void NGLineBreaker::Rewind(NGInlineItemResults* item_results,
                           unsigned new_end) {
  // TODO(kojii): Should we keep results for the next line? We don't need to
  // re-layout atomic inlines.
  // TODO(kojii): Removing processed floats is likely a problematic. Keep
  // floats in this line, or keep it for the next line.
  item_results->Shrink(new_end);

  MoveToNextOf(item_results->back());
}

void NGLineBreaker::UpdateBreakIterator(const ComputedStyle& style) {
  auto_wrap_ = style.AutoWrap();

  if (auto_wrap_) {
    break_iterator_.SetLocale(style.LocaleForLineBreakIterator());

    if (style.WordBreak() == EWordBreak::kBreakAll ||
        style.WordBreak() == EWordBreak::kBreakWord) {
      break_iterator_.SetBreakType(LineBreakType::kBreakAll);
    } else if (style.WordBreak() == EWordBreak::kKeepAll) {
      break_iterator_.SetBreakType(LineBreakType::kKeepAll);
    } else {
      break_iterator_.SetBreakType(LineBreakType::kNormal);
    }

    // TODO(kojii): Implement word-wrap/overflow-wrap property
  }
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
