// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_line_breaker.h"

#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/ShapingLineBreaker.h"

namespace blink {

NGLineBreaker::NGLineBreaker(
    NGInlineNode node,
    const NGConstraintSpace& space,
    NGFragmentBuilder* container_builder,
    Vector<RefPtr<NGUnpositionedFloat>>* unpositioned_floats,
    const NGInlineBreakToken* break_token)
    : node_(node),
      constraint_space_(space),
      container_builder_(container_builder),
      unpositioned_floats_(unpositioned_floats),
      break_iterator_(node.Text()),
      shaper_(node.Text().Characters16(), node.Text().length()),
      spacing_(node.Text()),
      base_direction_(node_.BaseDirection()) {
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

// Compute the base direction for bidi algorithm for this line.
void NGLineBreaker::ComputeBaseDirection() {
  // If 'unicode-bidi' is not 'plaintext', use the base direction of the block.
  if (!line_.is_after_forced_break ||
      node_.Style().GetUnicodeBidi() != UnicodeBidi::kPlaintext)
    return;
  // If 'unicode-bidi: plaintext', compute the base direction for each paragraph
  // (separated by forced break.)
  const String& text = node_.Text();
  if (text.Is8Bit())
    return;
  size_t end_offset = text.find(kNewlineCharacter, offset_);
  base_direction_ = NGBidiParagraph::BaseDirectionForString(node_.Text(
      offset_, end_offset == kNotFound ? text.length() : end_offset));
}

// Initialize internal states for the next line.
void NGLineBreaker::PrepareNextLine(const NGExclusionSpace& exclusion_space,
                                    NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  item_results->clear();
  line_info->SetStartOffset(offset_);
  line_info->SetLineStyle(node_, constraint_space_, IsFirstFormattedLine(),
                          line_.is_after_forced_break);
  SetCurrentStyle(line_info->LineStyle());
  ComputeBaseDirection();
  line_info->SetBaseDirection(base_direction_);

  line_.is_after_forced_break = false;
  line_.should_create_line_box = false;

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  line_.position = line_info->TextIndent();

  line_.exclusion_space = WTF::MakeUnique<NGExclusionSpace>(exclusion_space);

  // We are only able to calculate our available_width if our container has
  // been positioned in the BFC coordinate space yet.
  if (container_builder_->BfcOffset())
    FindNextLayoutOpportunity();
  else
    line_.opportunity.reset();
}

bool NGLineBreaker::NextLine(const NGLogicalOffset& content_offset,
                             const NGExclusionSpace& exclusion_space,
                             NGLineInfo* line_info) {
  content_offset_ = content_offset;

  PrepareNextLine(exclusion_space, line_info);
  BreakLine(line_info);
  line_info->SetEndOffset(offset_);

  // TODO(kojii): When editing, or caret is enabled, trailing spaces at wrap
  // point should not be removed. For other cases, we can a) remove, b) leave
  // characters without glyphs, or c) leave both characters and glyphs without
  // measuring. Need to decide which one works the best.
  SkipCollapsibleWhitespaces();

  if (line_info->Results().IsEmpty())
    return false;

  // TODO(kojii): There are cases where we need to PlaceItems() without creating
  // line boxes. These cases need to be reviewed.
  if (line_.should_create_line_box) {
    if (!line_.CanFit() &&
        node_.GetLayoutBlockFlow()->ShouldTruncateOverflowingText()) {
      TruncateOverflowingText(line_info);
    }

    ComputeLineLocation(line_info);
  }

  return true;
}

void NGLineBreaker::BreakLine(NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  const Vector<NGInlineItem>& items =
      node_.Items(line_info->UseFirstLineStyle());
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
        NGInlineItemResult(&item, item_index_, offset_, item.EndOffset()));
    NGInlineItemResult* item_result = &item_results->back();
    if (item.Type() == NGInlineItem::kText) {
      state = HandleText(line_info, item, item_result);
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
    } else if (item.Length()) {
      // For other items with text (e.g., bidi controls), use their text to
      // determine the break opportunity.
      item_result->prohibit_break_after =
          !break_iterator_.IsBreakable(item_result->end_offset);
      state = item_result->prohibit_break_after ? LineBreakState::kNotBreakable
                                                : LineBreakState::kIsBreakable;
      MoveToNextOf(item);
    } else {
      if (item.Type() == NGInlineItem::kListMarker)
        line_.should_create_line_box = true;
      item_result->prohibit_break_after = true;
      state = LineBreakState::kNotBreakable;
      MoveToNextOf(item);
    }
  }
  if (line_.HasAvailableWidth() && !line_.CanFit())
    return HandleOverflow(line_info);
  line_info->SetIsLastLine(true);
}

// @return if there are floats that affect current line.
// This is different from the clearance offset in that floats outside of the
// current layout opportunities, such as floats in margin/padding, or floats
// below such floats, are not included.
bool NGLineBreaker::HasFloatsAffectingCurrentLine() const {
  return line_.opportunity->InlineSize() !=
         constraint_space_.AvailableSize().inline_size;
}

// Update the inline size of the first layout opportunity from the given
// content_offset.
void NGLineBreaker::FindNextLayoutOpportunity() {
  const NGBfcOffset& bfc_offset = container_builder_->BfcOffset().value();

  NGBfcOffset origin_offset = {
      bfc_offset.line_offset,
      bfc_offset.block_offset + content_offset_.block_offset};

  line_.opportunity = line_.exclusion_space->FindLayoutOpportunity(
      origin_offset, constraint_space_.AvailableSize(),
      /* minimum_size */ NGLogicalSize());

  // When floats/exclusions occupies the entire line (e.g., float: left; width:
  // 100%), zero-inline-size opportunities are not included in the iterator.
  // Instead, the block offset of the first opportunity is pushed down to avoid
  // such floats/exclusions. Set the line box location to it.
  content_offset_.block_offset =
      line_.opportunity.value().BlockStartOffset() - bfc_offset.block_offset;
}

// Finds a layout opportunity that has the given minimum inline size, or the one
// without floats/exclusions (and that there will not be wider oppotunities than
// that,) and moves |content_offset_.block_offset| down to it.
//
// Used to move lines down when no break opportunities can fit in a line that
// has floats.
void NGLineBreaker::FindNextLayoutOpportunityWithMinimumInlineSize(
    LayoutUnit min_inline_size) {
  const NGBfcOffset& bfc_offset = container_builder_->BfcOffset().value();

  NGBfcOffset origin_offset = {
      bfc_offset.line_offset,
      bfc_offset.block_offset + content_offset_.block_offset};

  NGLogicalSize minimum_size(min_inline_size, LayoutUnit());
  line_.opportunity = line_.exclusion_space->FindLayoutOpportunity(
      origin_offset, constraint_space_.AvailableSize(), minimum_size);

  content_offset_.block_offset =
      line_.opportunity.value().BlockStartOffset() - bfc_offset.block_offset;
}

void NGLineBreaker::ComputeLineLocation(NGLineInfo* line_info) const {
  LayoutUnit line_left = line_.opportunity.value().LineStartOffset() -
                         constraint_space_.BfcOffset().line_offset;
  LayoutUnit available_width = line_.AvailableWidth();

  // Indenting should move the current position to compute the size of
  // tabulations. Since it is computed and stored in NGInlineItemResult,
  // adjust the positoin of the line box to simplify placing items.
  if (LayoutUnit text_indent = line_info->TextIndent()) {
    // Move the line box by indent. Negative indents are ink overflow, let the
    // line box overflow from the container box.
    if (IsLtr(line_info->BaseDirection()))
      line_left += text_indent;
    available_width -= text_indent;
  }

  line_info->SetLineOffset({line_left, content_offset_.block_offset},
                           available_width);
}

bool NGLineBreaker::IsFirstBreakOpportunity(unsigned offset,
                                            const NGLineInfo& line_info) const {
  return break_iterator_.NextBreakOpportunity(line_info.StartOffset() + 1) >=
         offset;
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
    NGLineInfo* line_info,
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
    BreakText(item_result, item, available_width - line_.position, line_info);
    LayoutUnit next_position = line_.position + item_result->inline_size;
    bool is_overflow = next_position > available_width;
    line_.position = next_position;
    item_result->no_break_opportunities_inside = is_overflow;
    if (item_result->end_offset < item.EndOffset())
      offset_ = item_result->end_offset;
    else
      MoveToNextOf(item);
    if (item_result->end_offset < item.EndOffset() ||
        item_result->has_hanging_spaces) {
      // The break point found. If it fits, break this line after including
      // trailing objects (end margins etc.)
      if (!is_overflow)
        return LineBreakState::kBreakAfterTrailings;

      // If overflow, proceed to the possible break point before start
      // rewinding. TODO(kojii): This could be more efficient if we knew there
      // was a break opportunity and that rewind will succeed.
      return LineBreakState::kIsBreakable;
    }

    DCHECK(is_overflow || item_result->start_offset != item.StartOffset());
    return item_result->prohibit_break_after ? LineBreakState::kNotBreakable
                                             : LineBreakState::kIsBreakable;
  }

  // Add the rest of the item if !auto_wrap.
  // Because the start position may need to reshape, run ShapingLineBreaker
  // with max available width.
  DCHECK_NE(offset_, item.StartOffset());
  BreakText(item_result, item, LayoutUnit::Max(), line_info);
  DCHECK_EQ(item_result->end_offset, item.EndOffset());
  item_result->no_break_opportunities_inside = true;
  item_result->prohibit_break_after = true;
  line_.position += item_result->inline_size;
  MoveToNextOf(item);
  return LineBreakState::kNotBreakable;
}

void NGLineBreaker::BreakText(NGInlineItemResult* item_result,
                              const NGInlineItem& item,
                              LayoutUnit available_width,
                              NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  item.AssertOffset(item_result->start_offset);

  // TODO(kojii): We need to instantiate ShapingLineBreaker here because it
  // has item-specific info as context. Should they be part of ShapeLine() to
  // instantiate once, or is this just fine since instatiation is not
  // expensive?
  DCHECK_EQ(item.TextShapeResult()->StartIndexForResult(), item.StartOffset());
  DCHECK_EQ(item.TextShapeResult()->EndIndexForResult(), item.EndOffset());
  ShapingLineBreaker breaker(&shaper_, &item.Style()->GetFont(),
                             item.TextShapeResult(), &break_iterator_,
                             &spacing_, hyphenation_);
  if (!enable_soft_hyphen_)
    breaker.DisableSoftHyphen();
  available_width = std::max(LayoutUnit(0), available_width);
  ShapingLineBreaker::Result result;
  RefPtr<ShapeResult> shape_result =
      breaker.ShapeLine(item_result->start_offset, available_width, &result);
  if (result.has_hanging_spaces) {
    item_result->has_hanging_spaces = true;
    // Hanging spaces do not expand min-content. Handle them simliar to visual
    // overflow. Some details are different, but it's the closest behavior.
    item_result->inline_size = available_width;
    DCHECK(!result.is_hyphenated);
  } else {
    item_result->inline_size = shape_result->SnappedWidth();

    // If overflow and no break opportunities exist, and if 'word-wrap:
    // break-word', try to break at every grapheme cluster boundary.
    if (item_result->inline_size > available_width && break_if_overflow_ &&
        break_iterator_.BreakType() == LineBreakType::kNormal &&
        IsFirstBreakOpportunity(result.break_offset, *line_info)) {
      break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
      BreakText(item_result, item, available_width, line_info);
      break_iterator_.SetBreakType(LineBreakType::kNormal);
      return;
    }

    if (result.is_hyphenated) {
      AppendHyphen(*item.Style(), line_info);
      item_result->inline_size = shape_result->SnappedWidth();
      // TODO(kojii): Implement when adding a hyphen caused overflow.
      // crbug.com/714962: Should be removed when switched to NGPaint.
      item_result->text_end_effect = NGTextEndEffect::kHyphen;
    }
  }
  item_result->end_offset = result.break_offset;
  item_result->shape_result = std::move(shape_result);
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

void NGLineBreaker::AppendHyphen(const ComputedStyle& style,
                                 NGLineInfo* line_info) {
  TextDirection direction = style.Direction();
  String hyphen_string = style.HyphenString();
  hyphen_string.Ensure16Bit();
  HarfBuzzShaper shaper(hyphen_string.Characters16(), hyphen_string.length());
  RefPtr<ShapeResult> hyphen_result = shaper.Shape(&style.GetFont(), direction);
  line_info->SetLineEndShapeResult(std::move(hyphen_result), &style);
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
NGLineBreaker::LineBreakState NGLineBreaker::HandleControlItem(
    const NGInlineItem& item,
    NGInlineItemResult* item_result) {
  DCHECK_EQ(item.Length(), 1u);
  line_.should_create_line_box = true;

  UChar character = node_.Text()[item.StartOffset()];
  switch (character) {
    case kNewlineCharacter:
      MoveToNextOf(item);
      return LineBreakState::kForcedBreak;
    case kTabulationCharacter: {
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      const Font& font = style.GetFont();
      item_result->inline_size =
          font.TabWidth(style.GetTabSize(), line_.position);
      line_.position += item_result->inline_size;
      MoveToNextOf(item);
      // TODO(kojii): Implement break around the tab character.
      return LineBreakState::kIsBreakable;
    }
    case kZeroWidthSpaceCharacter:
      // <wbr> tag creates break opportunities regardless of auto_wrap.
      MoveToNextOf(item);
      return LineBreakState::kIsBreakable;
  }
  NOTREACHED();
  return LineBreakState::kIsBreakable;
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleAtomicInline(
    const NGInlineItem& item,
    NGInlineItemResult* item_result,
    const NGLineInfo& line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  line_.should_create_line_box = true;

  item_result->layout_result =
      NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
          .LayoutAtomicInline(constraint_space_, line_info.UseFirstLineStyle());
  DCHECK(item_result->layout_result->PhysicalFragment());

  item_result->inline_size =
      NGFragment(constraint_space_.WritingMode(),
                 *item_result->layout_result->PhysicalFragment())
          .InlineSize();

  DCHECK(item.Style());
  item_result->margins =
      ComputeMarginsForVisualContainer(constraint_space_, *item.Style());
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
  // When rewind occurs, an item may be handled multiple times.
  // Since floats are put into a separate list, avoid handling same floats
  // twice.
  // Ideally rewind can take floats out of floats list, but the difference is
  // sutble compared to the complexity.
  // TODO(kojii): Keep a list of floats in a separate vector, then "commit" them
  // inside NGLineLayoutAlgorithm.
  if (item_index_ < handled_floats_end_item_index_) {
    MoveToNextOf(item);
    return ComputeIsBreakableAfter(item_result);
  }
  handled_floats_end_item_index_ = item_index_ + 1;

  NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));

  const ComputedStyle& float_style = node.Style();
  NGBoxStrut margins =
      ComputeMarginsForContainer(constraint_space_, float_style);

  // TODO(ikilpatrick): Add support for float break tokens inside an inline
  // layout context.
  RefPtr<NGUnpositionedFloat> unpositioned_float = NGUnpositionedFloat::Create(
      constraint_space_.AvailableSize(),
      constraint_space_.PercentageResolutionSize(),
      constraint_space_.BfcOffset().line_offset,
      constraint_space_.BfcOffset().line_offset, margins, node,
      /* break_token */ nullptr);

  LayoutUnit inline_size = ComputeInlineSizeForUnpositionedFloat(
      constraint_space_, unpositioned_float.get());

  // We can only determine if our float will fit if we have an available_width
  // I.e. we may not have come across any text yet, in order to be able to
  // resolve the BFC position.
  bool float_does_not_fit = (!constraint_space_.FloatsBfcOffset() ||
                             container_builder_->BfcOffset()) &&
                            (!line_.HasAvailableWidth() ||
                             !line_.CanFit(inline_size + margins.InlineSum()));

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!unpositioned_floats_->IsEmpty() || float_does_not_fit) {
    unpositioned_floats_->push_back(std::move(unpositioned_float));
  } else {
    NGBfcOffset container_bfc_offset =
        container_builder_->BfcOffset()
            ? container_builder_->BfcOffset().value()
            : constraint_space_.FloatsBfcOffset().value();
    LayoutUnit origin_block_offset =
        container_bfc_offset.block_offset + content_offset_.block_offset;

    NGPositionedFloat positioned_float = PositionFloat(
        origin_block_offset, container_bfc_offset.block_offset,
        unpositioned_float.get(), constraint_space_,
        container_builder_->Size().inline_size, line_.exclusion_space.get());
    container_builder_->AddChild(positioned_float.layout_result,
                                 positioned_float.logical_offset);

    // We need to recalculate the available_width as the float probably
    // consumed space on the line.
    if (container_builder_->BfcOffset())
      FindNextLayoutOpportunity();
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
    NGBoxStrut borders = ComputeBorders(constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(constraint_space_, style);
    item_result->borders_paddings_block_start =
        borders.block_start + paddings.block_start;
    item_result->borders_paddings_block_end =
        borders.block_end + paddings.block_end;
    if (item.HasStartEdge()) {
      item_result->margins =
          ComputeMarginsForContainer(constraint_space_, style);
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
      NGInlineItemResult(&item, item_index_, offset_, item.EndOffset()));
  NGInlineItemResult* item_result = &item_results->back();

  item_result->needs_box_when_empty = false;
  if (item.HasEndEdge()) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    item_result->margins = ComputeMarginsForContainer(constraint_space_, style);
    NGBoxStrut borders = ComputeBorders(constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(constraint_space_, style);
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
  HandleOverflow(line_info, line_.AvailableWidth(), false);
}

void NGLineBreaker::HandleOverflow(NGLineInfo* line_info,
                                   LayoutUnit available_width,
                                   bool force_break_anywhere) {
  NGInlineItemResults* item_results = &line_info->Results();
  LayoutUnit width_to_rewind = line_.position - available_width;
  DCHECK_GT(width_to_rewind, 0);

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results)[--i];

    // Try to break after this item.
    if (i < item_results->size() - 1 && !item_result->prohibit_break_after) {
      if (width_to_rewind <= 0) {
        line_.position = available_width + width_to_rewind;
        return Rewind(line_info, i + 1);
      }
      break_before = i + 1;
    }

    // Try to break inside of this item.
    LayoutUnit next_width_to_rewind =
        width_to_rewind - item_result->inline_size;
    DCHECK(item_result->item);
    const NGInlineItem& item = *item_result->item;
    if (item.Type() == NGInlineItem::kText && next_width_to_rewind < 0 &&
        (!item_result->no_break_opportunities_inside || force_break_anywhere)) {
      // When the text fits but its right margin does not, the break point
      // must not be at the end.
      LayoutUnit item_available_width =
          std::min(-next_width_to_rewind, item_result->inline_size - 1);
      SetCurrentStyle(*item.Style());
      if (force_break_anywhere)
        break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
      BreakText(item_result, item, item_available_width, line_info);
      if (item_result->inline_size <= item_available_width) {
        DCHECK(item_result->end_offset < item.EndOffset() ||
               (item_result->end_offset == item.EndOffset() &&
                item_result->has_hanging_spaces));
        DCHECK(!item_result->prohibit_break_after);
        line_.position =
            available_width + next_width_to_rewind + item_result->inline_size;
        DCHECK_LE(i + 1, item_results->size());
        if (i + 1 < item_results->size())
          Rewind(line_info, i + 1);
        return;
      }
    }

    width_to_rewind = next_width_to_rewind;
  }

  // Reaching here means that the rewind point was not found.

  // When the first break opportunity overflows and if the current line has
  // floats or intruding floats, we need to find the next layout opportunity
  // which will fit the first break opportunity.
  // Doing so will move the line down to where there are narrower floats (and
  // thus wider available width,) or no floats.
  if (HasFloatsAffectingCurrentLine()) {
    FindNextLayoutOpportunityWithMinimumInlineSize(line_.position);
    // Moving the line down widened the available width. Need to rewind items
    // that depend on old available width, but it's not trivial to rewind all
    // the states. For the simplicity, rewind to the beginning of the line.
    Rewind(line_info, 0);
    SetCurrentStyle(line_info->LineStyle());
    line_.position = line_info->TextIndent();
    BreakLine(line_info);
    return;
  }

  // Let this line overflow.
  // If there was a break opporunity, the overflow should stop there.
  if (break_before)
    return Rewind(line_info, break_before);

  line_info->SetIsLastLine(item_index_ >= node_.Items().size());
}

void NGLineBreaker::Rewind(NGLineInfo* line_info, unsigned new_end) {
  NGInlineItemResults* item_results = &line_info->Results();
  DCHECK_LT(new_end, item_results->size());

  // TODO(ikilpatrick): Add rewinding of exclusions.

  if (new_end) {
    // Use |results[new_end - 1].end_offset| because it may have been truncated
    // and may not be equal to |results[new_end].start_offset|.
    MoveToNextOf((*item_results)[new_end - 1]);
  } else {
    // When rewinding all items, use |results[0].start_offset|.
    const NGInlineItemResult& first_remove = (*item_results)[new_end];
    item_index_ = first_remove.item_index;
    offset_ = first_remove.start_offset;
  }

  // TODO(kojii): Should we keep results for the next line? We don't need to
  // re-layout atomic inlines.
  // TODO(kojii): Removing processed floats is likely a problematic. Keep
  // floats in this line, or keep it for the next line.
  item_results->Shrink(new_end);

  line_info->SetIsLastLine(false);
}

// Truncate overflowing text and append ellipsis.
void NGLineBreaker::TruncateOverflowingText(NGLineInfo* line_info) {
  // The ellipsis is styled according to the line style.
  const Font& font = line_info->LineStyle().GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  DCHECK(font_data);
  String ellipsis =
      font_data && font_data->GlyphForCharacter(kHorizontalEllipsisCharacter)
          ? String(&kHorizontalEllipsisCharacter, 1)
          : String(u"...");
  HarfBuzzShaper shaper(ellipsis.Characters16(), ellipsis.length());
  RefPtr<ShapeResult> shape_result =
      shaper.Shape(&font, line_info->BaseDirection());

  // Truncate the line to (available_width - ellipsis_width) using 'line-break:
  // anywhere'.
  unsigned saved_item_index = item_index_;
  unsigned saved_offset = offset_;
  HandleOverflow(line_info,
                 line_.AvailableWidth() - shape_result->SnappedWidth(), true);

  // Restore item_index/offset to before HandleOverflow().
  item_index_ = saved_item_index;
  offset_ = saved_offset;

  // The ellipsis should appear at the logical end of the line.
  // This is stored seprately from other results so that it can be appended
  // after bidi reorder.
  line_info->SetLineEndShapeResult(std::move(shape_result),
                                   &line_info->LineStyle());
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
    break_iterator_.SetBreakSpace(style.BreakOnlyAfterWhiteSpace()
                                      ? BreakSpaceType::kAfter
                                      : BreakSpaceType::kBeforeSpace);

    enable_soft_hyphen_ = style.GetHyphens() != Hyphens::kNone;
    hyphenation_ = style.GetHyphenation();
  }

  spacing_.SetSpacing(style.GetFontDescription());
}

void NGLineBreaker::MoveToNextOf(const NGInlineItem& item) {
  offset_ = item.EndOffset();
  item_index_++;
}

void NGLineBreaker::MoveToNextOf(const NGInlineItemResult& item_result) {
  offset_ = item_result.end_offset;
  item_index_ = item_result.item_index;
  DCHECK(item_result.item);
  if (offset_ == item_result.item->EndOffset())
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
