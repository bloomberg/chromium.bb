// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_line_breaker.h"

#include "core/layout/ng/inline/ng_bidi_paragraph.h"
#include "core/layout/ng/inline/ng_inline_break_token.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/inline/ng_text_fragment_builder.h"
#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floats_utils.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_positioned_float.h"
#include "core/style/ComputedStyle.h"
#include "platform/fonts/shaping/ShapingLineBreaker.h"

namespace blink {

namespace {

// CSS-defined white space characters, excluding the newline character.
// In most cases, the line breaker consider break opportunities are before
// spaces because it handles trailing spaces differently from other normal
// characters, but breaking before newline characters is not desired.
inline bool IsBreakableSpace(UChar c) {
  return c == kSpaceCharacter || c == kTabulationCharacter;
}

inline bool CanBreakAfterLast(const NGInlineItemResults& item_results) {
  return !item_results.IsEmpty() && item_results.back().can_break_after;
}

}  // namespace

NGLineBreaker::NGLineBreaker(
    NGInlineNode node,
    NGLineBreakerMode mode,
    const NGConstraintSpace& space,
    Vector<NGPositionedFloat>* positioned_floats,
    Vector<scoped_refptr<NGUnpositionedFloat>>* unpositioned_floats,
    NGExclusionSpace* exclusion_space,
    unsigned handled_float_index,
    const NGInlineBreakToken* break_token)
    : node_(node),
      mode_(mode),
      constraint_space_(space),
      positioned_floats_(positioned_floats),
      unpositioned_floats_(unpositioned_floats),
      exclusion_space_(exclusion_space),
      break_iterator_(node.Text()),
      shaper_(node.Text().Characters16(), node.Text().length()),
      spacing_(node.Text()),
      handled_floats_end_item_index_(handled_float_index),
      base_direction_(node_.BaseDirection()),
      in_line_height_quirks_mode_(node.InLineHeightQuirksMode()) {
  break_iterator_.SetBreakSpace(BreakSpaceType::kBeforeSpaceRun);

  if (break_token) {
    current_style_ = break_token->Style();
    item_index_ = break_token->ItemIndex();
    offset_ = break_token->TextOffset();
    previous_line_had_forced_break_ = break_token->IsForcedBreak();
    node.AssertOffset(item_index_, offset_);
    ignore_floats_ = break_token->IgnoreFloats();
  }
}

inline NGInlineItemResult* NGLineBreaker::AddItem(
    const NGInlineItem& item,
    unsigned end_offset,
    NGInlineItemResults* item_results) {
  DCHECK_LE(end_offset, item.EndOffset());
  item_results->push_back(
      NGInlineItemResult(&item, item_index_, offset_, end_offset));
  return &item_results->back();
}

inline NGInlineItemResult* NGLineBreaker::AddItem(
    const NGInlineItem& item,
    NGInlineItemResults* item_results) {
  return AddItem(item, item.EndOffset(), item_results);
}

void NGLineBreaker::SetLineEndFragment(
    scoped_refptr<NGPhysicalTextFragment> fragment,
    NGLineInfo* line_info) {
  bool is_horizontal =
      IsHorizontalWritingMode(constraint_space_.GetWritingMode());
  if (line_info->LineEndFragment()) {
    const NGPhysicalSize& size = line_info->LineEndFragment()->Size();
    line_.position -= is_horizontal ? size.width : size.height;
  }
  if (fragment) {
    const NGPhysicalSize& size = fragment->Size();
    line_.position += is_horizontal ? size.width : size.height;
  }
  line_info->SetLineEndFragment(std::move(fragment));
}

inline void NGLineBreaker::ComputeCanBreakAfter(
    NGInlineItemResult* item_result) const {
  item_result->can_break_after =
      auto_wrap_ && break_iterator_.IsBreakable(item_result->end_offset);
}

// @return if this is the "first formatted line".
// https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
bool NGLineBreaker::IsFirstFormattedLine() const {
  if (item_index_ || offset_)
    return false;

  // TODO(kojii): In LayoutNG, leading OOF creates an anonymous block box,
  // and that |CanContainFirstFormattedLine()| does not work.
  // crbug.com/734554
  // return node_.GetLayoutBlockFlow()->CanContainFirstFormattedLine();
  LayoutObject* layout_object = node_.GetLayoutBlockFlow();
  if (!layout_object->IsAnonymousBlock())
    return true;
  for (;;) {
    layout_object = layout_object->PreviousSibling();
    if (!layout_object)
      return true;
    if (!layout_object->IsFloatingOrOutOfFlowPositioned())
      return false;
  }
}

// Compute the base direction for bidi algorithm for this line.
void NGLineBreaker::ComputeBaseDirection() {
  // If 'unicode-bidi' is not 'plaintext', use the base direction of the block.
  if (!previous_line_had_forced_break_ ||
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
void NGLineBreaker::PrepareNextLine(const NGLayoutOpportunity& opportunity,
                                    NGLineInfo* line_info) {
  NGInlineItemResults* item_results = &line_info->Results();
  item_results->clear();
  line_info->SetStartOffset(offset_);
  line_info->SetLineStyle(node_, constraint_space_, IsFirstFormattedLine(),
                          previous_line_had_forced_break_);
  // Set the initial style of this line from the break token. Example:
  //   <p>...<span>....</span></p>
  // When the line wraps in <span>, the 2nd line needs to start with the style
  // of the <span>.
  override_break_anywhere_ = false;
  SetCurrentStyle(current_style_ ? *current_style_ : line_info->LineStyle());
  ComputeBaseDirection();
  line_info->SetBaseDirection(base_direction_);

  line_.is_after_forced_break = false;
  line_.should_create_line_box = false;

  // Use 'text-indent' as the initial position. This lets tab positions to align
  // regardless of 'text-indent'.
  line_.position = line_info->TextIndent();

  line_.opportunity = opportunity;
  line_.line_left_bfc_offset = opportunity.rect.LineStartOffset();
  line_.line_right_bfc_offset = opportunity.rect.LineEndOffset();
  bfc_block_offset_ = opportunity.rect.BlockStartOffset();
}

bool NGLineBreaker::NextLine(const NGLayoutOpportunity& opportunity,
                             NGLineInfo* line_info) {
  bfc_block_offset_ = constraint_space_.BfcOffset().block_offset;

  PrepareNextLine(opportunity, line_info);
  BreakLine(line_info);

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
  LineBreakState state = LineBreakState::kContinue;
  while (state != LineBreakState::kDone) {
    // Check overflow even if |item_index_| is at the end of the block, because
    // the last item of the block may have caused overflow. In that case,
    // |HandleOverflow| will rewind |item_index_|.
    if (state == LineBreakState::kContinue && auto_wrap_ && !line_.CanFit()) {
      state = HandleOverflow(line_info);
    }

    // If we reach at the end of the block, this is the last line.
    DCHECK_LE(item_index_, items.size());
    if (item_index_ == items.size()) {
      line_info->SetIsLastLine(true);
      return;
    }

    // Handle trailable items first. These items may not be break before.
    // They (or part of them) may also overhang the available width.
    const NGInlineItem& item = items[item_index_];
    if (item.Type() == NGInlineItem::kText) {
      state = HandleText(item, state, line_info);
#if DCHECK_IS_ON()
      if (!item_results->IsEmpty())
        item_results->back().CheckConsistency();
#endif
      continue;
    }
    if (item.Type() == NGInlineItem::kCloseTag) {
      HandleCloseTag(item, item_results);
      continue;
    }
    if (item.Type() == NGInlineItem::kControl) {
      state = HandleControlItem(item, state, line_info);
      continue;
    }
    if (item.Type() == NGInlineItem::kBidiControl) {
      state = HandleBidiControlItem(item, state, line_info);
      continue;
    }

    // Items after this point are not trailable. Break at the earliest break
    // opportunity if we're trailing.
    if (state == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results)) {
      line_info->SetIsLastLine(false);
      return;
    }

    if (item.Type() == NGInlineItem::kAtomicInline) {
      HandleAtomicInline(item, line_info);
    } else if (item.Type() == NGInlineItem::kOpenTag) {
      HandleOpenTag(item, AddItem(item, item_results));
    } else if (item.Type() == NGInlineItem::kFloating) {
      HandleFloat(item, AddItem(item, item_results));
    } else if (item.Type() == NGInlineItem::kOutOfFlowPositioned) {
      DCHECK_EQ(item.Length(), 0u);
      AddItem(item, item_results);
      MoveToNextOf(item);
    } else if (item.Length()) {
      NOTREACHED();
      // For other items with text (e.g., bidi controls), use their text to
      // determine the break opportunity.
      NGInlineItemResult* item_result = AddItem(item, item_results);
      item_result->can_break_after =
          break_iterator_.IsBreakable(item_result->end_offset);
      MoveToNextOf(item);
    } else if (item.Type() == NGInlineItem::kListMarker) {
      line_.should_create_line_box = true;
      NGInlineItemResult* item_result = AddItem(item, item_results);
      DCHECK(!item_result->can_break_after);
      MoveToNextOf(item);
    } else {
      NOTREACHED();
      MoveToNextOf(item);
    }
  }
}

// Re-compute the current position from NGInlineItemResults.
// The current position is usually updated as NGLineBreaker builds
// NGInlineItemResults. This function re-computes it when it was lost.
void NGLineBreaker::UpdatePosition(const NGInlineItemResults& results) {
  LayoutUnit position;
  for (const NGInlineItemResult& item_result : results)
    position += item_result.inline_size;
  line_.position = position;
}

void NGLineBreaker::ComputeLineLocation(NGLineInfo* line_info) const {
  LayoutUnit bfc_line_offset = line_.line_left_bfc_offset;
  LayoutUnit available_width = line_.AvailableWidth();

  // Negative margins can make the position negative, but the inline size is
  // always positive or 0.
  line_info->SetLineBfcOffset({bfc_line_offset, bfc_block_offset_},
                              available_width,
                              line_.position.ClampNegativeToZero());
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleText(
    const NGInlineItem& item,
    LineBreakState state,
    NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  DCHECK(item.TextShapeResult());
  NGInlineItemResults* item_results = &line_info->Results();

  // If we're trailing, only trailing spaces can be included in this line.
  if (state == LineBreakState::kTrailing && CanBreakAfterLast(*item_results)) {
    return HandleTrailingSpaces(item, line_info);
  }

  line_.should_create_line_box = true;
  NGInlineItemResult* item_result = AddItem(item, item_results);
  LayoutUnit available_width = line_.AvailableWidth();

  if (auto_wrap_) {
    // Try to break inside of this text item.
    BreakText(item_result, item, available_width - line_.position, line_info);
    LayoutUnit next_position = line_.position + item_result->inline_size;
    bool is_overflow = next_position > available_width;
    line_.position = next_position;
    item_result->may_break_inside = !is_overflow;
    MoveToNextOf(*item_result);

    if (!is_overflow || state == LineBreakState::kTrailing) {
      if (item_result->end_offset < item.EndOffset()) {
        // The break point found, and text follows. Break here.
        HandleTrailingSpaces(item, line_info);
        line_info->SetIsLastLine(false);
        return LineBreakState::kDone;
      }

      // The break point found, but items that prohibit breaking before them may
      // follow. Continue looking next items.
      return state;
    }

    return HandleOverflow(line_info);
  }

  // Add the rest of the item if !auto_wrap.
  // Because the start position may need to reshape, run ShapingLineBreaker
  // with max available width.
  BreakText(item_result, item, LayoutUnit::Max(), line_info);
  DCHECK_EQ(item_result->end_offset, item.EndOffset());
  DCHECK(!item_result->may_break_inside);
  item_result->can_break_after = false;
  line_.position += item_result->inline_size;
  MoveToNextOf(item);
  return state;
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
  scoped_refptr<ShapeResult> shape_result =
      breaker.ShapeLine(item_result->start_offset, available_width,
                        offset_ == line_info->StartOffset(), &result);
  DCHECK_GT(shape_result->NumCharacters(), 0u);
  if (result.is_hyphenated) {
    AppendHyphen(item, line_info);
    // TODO(kojii): Implement when adding a hyphen caused overflow.
    // crbug.com/714962: Should be removed when switched to NGPaint.
    item_result->text_end_effect = NGTextEndEffect::kHyphen;
  }
  item_result->inline_size = shape_result->SnappedWidth().ClampNegativeToZero();
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
    item_result->can_break_after = true;
  } else {
    DCHECK_EQ(item_result->end_offset, item.EndOffset());
    item_result->can_break_after =
        break_iterator_.IsBreakable(item_result->end_offset);
  }
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleTrailingSpaces(
    const NGInlineItem& item,
    NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kText);
  DCHECK_LT(offset_, item.EndOffset());
  const String& text = Text();
  NGInlineItemResults* item_results = &line_info->Results();
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  if (style.CollapseWhiteSpace()) {
    if (text[offset_] != kSpaceCharacter)
      return LineBreakState::kDone;

    // Skipping one whitespace removes all collapsible spaces because
    // collapsible spaces are collapsed to single space in NGInlineItemBuilder.
    offset_++;

    // Make the last item breakable after, even if it was nowrap.
    DCHECK(!item_results->IsEmpty());
    item_results->back().can_break_after = true;
  } else {
    // Find the end of the run of space characters in this item.
    // Other white space characters (e.g., tab) are not included in this item.
    DCHECK(style.BreakOnlyAfterWhiteSpace());
    unsigned end = offset_;
    while (end < item.EndOffset() && text[end] == kSpaceCharacter)
      end++;
    if (end == offset_)
      return LineBreakState::kDone;

    NGInlineItemResult* item_result = AddItem(item, end, item_results);
    item_result->has_only_trailing_spaces = true;
    item_result->shape_result = item.TextShapeResult()->SubRange(offset_, end);
    item_result->inline_size = item_result->shape_result->SnappedWidth();
    line_.position += item_result->inline_size;
    item_result->can_break_after =
        end < text.length() && !IsBreakableSpace(text[end]);
    offset_ = end;
  }

  // If non-space characters follow, the line is done.
  // Otherwise keep checking next items for the break point.
  DCHECK_LE(offset_, item.EndOffset());
  if (offset_ < item.EndOffset())
    return LineBreakState::kDone;
  item_index_++;
  return LineBreakState::kTrailing;
}

void NGLineBreaker::AppendHyphen(const NGInlineItem& item,
                                 NGLineInfo* line_info) {
  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  TextDirection direction = style.Direction();
  String hyphen_string = style.HyphenString();
  hyphen_string.Ensure16Bit();
  HarfBuzzShaper shaper(hyphen_string.Characters16(), hyphen_string.length());
  scoped_refptr<ShapeResult> hyphen_result =
      shaper.Shape(&style.GetFont(), direction);
  NGTextFragmentBuilder builder(node_, constraint_space_.GetWritingMode());
  builder.SetText(item.GetLayoutObject(), hyphen_string, &style,
                  std::move(hyphen_result));
  SetLineEndFragment(builder.ToTextFragment(), line_info);
}

// Measure control items; new lines and tab, that are similar to text, affect
// layout, but do not need shaping/painting.
NGLineBreaker::LineBreakState NGLineBreaker::HandleControlItem(
    const NGInlineItem& item,
    LineBreakState state,
    NGLineInfo* line_info) {
  DCHECK_EQ(item.Length(), 1u);
  line_.should_create_line_box = true;

  UChar character = Text()[item.StartOffset()];
  switch (character) {
    case kNewlineCharacter: {
      NGInlineItemResult* item_result = AddItem(item, &line_info->Results());
      item_result->has_only_trailing_spaces = true;
      line_.is_after_forced_break = true;
      line_info->SetIsLastLine(true);
      state = LineBreakState::kDone;
      break;
    }
    case kTabulationCharacter: {
      NGInlineItemResult* item_result = AddItem(item, &line_info->Results());
      DCHECK(item.Style());
      const ComputedStyle& style = *item.Style();
      const Font& font = style.GetFont();
      item_result->inline_size =
          font.TabWidth(style.GetTabSize(), line_.position);
      line_.position += item_result->inline_size;
      item_result->has_only_trailing_spaces =
          state == LineBreakState::kTrailing;
      ComputeCanBreakAfter(item_result);
      break;
    }
    case kZeroWidthSpaceCharacter: {
      // <wbr> tag creates break opportunities regardless of auto_wrap.
      NGInlineItemResult* item_result = AddItem(item, &line_info->Results());
      item_result->can_break_after = true;
      break;
    }
    case kCarriageReturnCharacter:
    case kFormFeedCharacter:
      // Ignore carriage return and form feed.
      // https://drafts.csswg.org/css-text-3/#white-space-processing
      // https://github.com/w3c/csswg-drafts/issues/855
      break;
    default:
      NOTREACHED();
      break;
  }
  MoveToNextOf(item);
  return state;
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleBidiControlItem(
    const NGInlineItem& item,
    LineBreakState state,
    NGLineInfo* line_info) {
  DCHECK_EQ(item.Length(), 1u);
  NGInlineItemResults* item_results = &line_info->Results();

  // Bidi control characters have enter/exit semantics. Handle "enter"
  // characters simialr to open-tag, while "exit" (pop) characters similar to
  // close-tag.
  UChar character = Text()[item.StartOffset()];
  bool is_pop = character == kPopDirectionalIsolateCharacter ||
                character == kPopDirectionalFormattingCharacter;
  if (is_pop) {
    if (!item_results->IsEmpty()) {
      NGInlineItemResult* item_result = AddItem(item, item_results);
      NGInlineItemResult* last = &(*item_results)[item_results->size() - 2];
      item_result->can_break_after = last->can_break_after;
      last->can_break_after = false;
    } else {
      AddItem(item, item_results);
    }
  } else {
    if (state == LineBreakState::kTrailing &&
        CanBreakAfterLast(*item_results)) {
      line_info->SetIsLastLine(false);
      MoveToNextOf(item);
      return LineBreakState::kDone;
    }
    NGInlineItemResult* item_result = AddItem(item, item_results);
    DCHECK(!item_result->can_break_after);
  }
  MoveToNextOf(item);
  return state;
}

void NGLineBreaker::HandleAtomicInline(const NGInlineItem& item,
                                       NGLineInfo* line_info) {
  DCHECK_EQ(item.Type(), NGInlineItem::kAtomicInline);
  line_.should_create_line_box = true;

  NGInlineItemResult* item_result = AddItem(item, &line_info->Results());
  item_result->layout_result =
      NGBlockNode(ToLayoutBox(item.GetLayoutObject()))
          .LayoutAtomicInline(constraint_space_,
                              line_info->UseFirstLineStyle());
  DCHECK(item_result->layout_result->PhysicalFragment());

  item_result->inline_size =
      NGFragment(constraint_space_.GetWritingMode(),
                 *item_result->layout_result->PhysicalFragment())
          .InlineSize();

  DCHECK(item.Style());
  item_result->margins =
      ComputeMarginsForVisualContainer(constraint_space_, *item.Style());
  item_result->padding = ComputePadding(constraint_space_, *item.Style());
  item_result->inline_size += item_result->margins.InlineSum();

  line_.position += item_result->inline_size;
  ComputeCanBreakAfter(item_result);
  MoveToNextOf(item);
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
void NGLineBreaker::HandleFloat(const NGInlineItem& item,
                                NGInlineItemResult* item_result) {
  // When rewind occurs, an item may be handled multiple times.
  // Since floats are put into a separate list, avoid handling same floats
  // twice.
  // Ideally rewind can take floats out of floats list, but the difference is
  // sutble compared to the complexity.
  //
  // Additionally, we need to skip floats if we're retrying a line after a
  // fragmentainer break. In that case the floats associated with this line will
  // already have been processed.
  ComputeCanBreakAfter(item_result);
  MoveToNextOf(item);
  if (item_index_ <= handled_floats_end_item_index_ || ignore_floats_)
    return;

  NGBlockNode node(ToLayoutBox(item.GetLayoutObject()));

  const ComputedStyle& float_style = node.Style();
  NGBoxStrut margins =
      ComputeMarginsForContainer(constraint_space_, float_style);

  // TODO(ikilpatrick): Add support for float break tokens inside an inline
  // layout context.
  scoped_refptr<NGUnpositionedFloat> unpositioned_float =
      NGUnpositionedFloat::Create(constraint_space_.AvailableSize(),
                                  constraint_space_.PercentageResolutionSize(),
                                  constraint_space_.BfcOffset().line_offset,
                                  constraint_space_.BfcOffset().line_offset,
                                  margins, node,
                                  /* break_token */ nullptr);

  LayoutUnit inline_margin_size =
      (ComputeInlineSizeForUnpositionedFloat(constraint_space_,
                                             unpositioned_float.get()) +
       margins.InlineSum())
          .ClampNegativeToZero();

  // The float should be positioned after the current line if:
  //  - It can't fit.
  //  - It will be moved down due to block-start edge alignment.
  //  - It will be moved down due to clearance.
  //  - We are currently computing our min/max-content size. (We use the
  //    unpositioned_floats to manually adjust the min/max-content size after
  //    the line breaker has run).
  bool float_after_line =
      !line_.CanFit(inline_margin_size) ||
      exclusion_space_->LastFloatBlockStart() > bfc_block_offset_ ||
      exclusion_space_->ClearanceOffset(float_style.Clear()) >
          bfc_block_offset_ ||
      mode_ != NGLineBreakerMode::kContent;

  // Check if we already have a pending float. That's because a float cannot be
  // higher than any block or floated box generated before.
  if (!unpositioned_floats_->IsEmpty() || float_after_line) {
    unpositioned_floats_->push_back(std::move(unpositioned_float));
  } else {
    LayoutUnit origin_block_offset = bfc_block_offset_;

    NGPositionedFloat positioned_float = PositionFloat(
        origin_block_offset, constraint_space_.BfcOffset().block_offset,
        unpositioned_float.get(), constraint_space_, exclusion_space_);
    positioned_floats_->push_back(positioned_float);

    DCHECK_EQ(positioned_float.bfc_offset.block_offset,
              bfc_block_offset_ + margins.block_start);

    if (float_style.Floating() == EFloat::kLeft) {
      line_.line_left_bfc_offset = std::max(
          line_.line_left_bfc_offset,
          positioned_float.bfc_offset.line_offset + inline_margin_size -
              margins.LineLeft(TextDirection::kLtr));
    } else {
      line_.line_right_bfc_offset =
          std::min(line_.line_right_bfc_offset,
                   positioned_float.bfc_offset.line_offset -
                       margins.LineLeft(TextDirection::kLtr));
    }

    DCHECK_GE(line_.line_left_bfc_offset, LayoutUnit());
    DCHECK_GE(line_.line_right_bfc_offset, LayoutUnit());
    DCHECK_GE(line_.AvailableWidth(), LayoutUnit());
  }
}

void NGLineBreaker::HandleOpenTag(const NGInlineItem& item,
                                  NGInlineItemResult* item_result) {
  DCHECK(!item_result->can_break_after);

  DCHECK(item.Style());
  const ComputedStyle& style = *item.Style();
  item_result->has_edge = item.HasStartEdge();
  if (item.ShouldCreateBoxFragment() &&
      (style.HasBorder() || style.HasPadding() ||
       (style.HasMargin() && item_result->has_edge))) {
    NGBoxStrut borders = ComputeBorders(constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(constraint_space_, style);
    item_result->padding = paddings;
    item_result->borders_paddings_block_start =
        borders.block_start + paddings.block_start;
    item_result->borders_paddings_block_end =
        borders.block_end + paddings.block_end;
    if (item_result->has_edge) {
      item_result->margins = ComputeMarginsForSelf(constraint_space_, style);
      item_result->inline_size = item_result->margins.inline_start +
                                 borders.inline_start + paddings.inline_start;
      line_.position += item_result->inline_size;

      // While the spec defines "non-zero margins, padding, or borders" prevents
      // line boxes to be zero-height, tests indicate that only inline direction
      // of them do so. See should_create_line_box_.
      // Force to create a box, because such inline boxes affect line heights.
      if (!line_.should_create_line_box &&
          (item_result->inline_size ||
           (item_result->margins.inline_start && !in_line_height_quirks_mode_)))
        line_.should_create_line_box = true;
    }
  }
  SetCurrentStyle(style);
  MoveToNextOf(item);
}

void NGLineBreaker::HandleCloseTag(const NGInlineItem& item,
                                   NGInlineItemResults* item_results) {
  NGInlineItemResult* item_result = AddItem(item, item_results);
  item_result->has_edge = item.HasEndEdge();
  if (item_result->has_edge) {
    DCHECK(item.Style());
    const ComputedStyle& style = *item.Style();
    item_result->margins = ComputeMarginsForSelf(constraint_space_, style);
    NGBoxStrut borders = ComputeBorders(constraint_space_, style);
    NGBoxStrut paddings = ComputePadding(constraint_space_, style);
    item_result->inline_size = item_result->margins.inline_end +
                               borders.inline_end + paddings.inline_end;
    line_.position += item_result->inline_size;

    if (!line_.should_create_line_box &&
        (item_result->inline_size ||
         (item_result->margins.inline_end && !in_line_height_quirks_mode_)))
      line_.should_create_line_box = true;
  }
  DCHECK(item.GetLayoutObject() && item.GetLayoutObject()->Parent());
  bool was_auto_wrap = auto_wrap_;
  SetCurrentStyle(item.GetLayoutObject()->Parent()->StyleRef());
  MoveToNextOf(item);

  // Prohibit break before a close tag by setting can_break_after to the
  // previous result.
  // TODO(kojii): There should be a result before close tag, but there are cases
  // that doesn't because of the way we handle trailing spaces. This needs to be
  // revisited.
  if (item_results->size() >= 2) {
    NGInlineItemResult* last = &(*item_results)[item_results->size() - 2];
    if (was_auto_wrap == auto_wrap_) {
      item_result->can_break_after = last->can_break_after;
      last->can_break_after = false;
      return;
    }
    last->can_break_after = false;
    if (!was_auto_wrap) {
      DCHECK(auto_wrap_);
      // When auto-wrap starts after no-wrap, the boundary is not allowed to
      // wrap. However, when space characters follow the boundary, there should
      // be a break opportunity after the space. The break_iterator cannot
      // compute this because it considers break opportunities are before a run
      // of spaces.
      const String& text = node_.Text();
      if (offset_ < text.length() && IsBreakableSpace(text[offset_])) {
        item_result->can_break_after = true;
        return;
      }
    }
  }
  ComputeCanBreakAfter(item_result);
}

// Handles when the last item overflows.
// At this point, item_results does not fit into the current line, and there
// are no break opportunities in item_results.back().
NGLineBreaker::LineBreakState NGLineBreaker::HandleOverflow(
    NGLineInfo* line_info) {
  return HandleOverflow(line_info, line_.AvailableWidth());
}

NGLineBreaker::LineBreakState NGLineBreaker::HandleOverflow(
    NGLineInfo* line_info,
    LayoutUnit available_width) {
  NGInlineItemResults* item_results = &line_info->Results();
  LayoutUnit width_to_rewind = line_.position - available_width;
  DCHECK_GT(width_to_rewind, 0);

  // Keep track of the shortest break opportunity.
  unsigned break_before = 0;

  // Search for a break opportunity that can fit.
  for (unsigned i = item_results->size(); i;) {
    NGInlineItemResult* item_result = &(*item_results)[--i];

    // Try to break after this item.
    if (i < item_results->size() - 1 && item_result->can_break_after) {
      if (width_to_rewind <= 0) {
        line_.position = available_width + width_to_rewind;
        Rewind(line_info, i + 1);
        return LineBreakState::kTrailing;
      }
      break_before = i + 1;
    }

    // Try to break inside of this item.
    LayoutUnit next_width_to_rewind =
        width_to_rewind - item_result->inline_size;
    DCHECK(item_result->item);
    const NGInlineItem& item = *item_result->item;
    if (item.Type() == NGInlineItem::kText && next_width_to_rewind < 0 &&
        (item_result->may_break_inside || override_break_anywhere_)) {
      // When the text fits but its right margin does not, the break point
      // must not be at the end.
      LayoutUnit item_available_width =
          std::min(-next_width_to_rewind, item_result->inline_size - 1);
      SetCurrentStyle(*item.Style());
      BreakText(item_result, item, item_available_width, line_info);
#if DCHECK_IS_ON()
      item_result->CheckConsistency();
#endif
      if (item_result->inline_size <= item_available_width) {
        DCHECK(item_result->end_offset < item.EndOffset());
        DCHECK(item_result->can_break_after);
        DCHECK_LE(i + 1, item_results->size());
        if (i + 1 == item_results->size()) {
          // If this is the last item, adjust states to accomodate the change.
          line_.position =
              available_width + next_width_to_rewind + item_result->inline_size;
          if (line_info->LineEndFragment())
            SetLineEndFragment(nullptr, line_info);
#if DCHECK_IS_ON()
          LayoutUnit position_fast = line_.position;
          UpdatePosition(line_info->Results());
          DCHECK_EQ(line_.position, position_fast);
#endif
          item_index_ = item_result->item_index;
          offset_ = item_result->end_offset;
          node_.AssertOffset(item_index_, offset_);
        } else {
          Rewind(line_info, i + 1);
        }
        return LineBreakState::kTrailing;
      }
    }

    width_to_rewind = next_width_to_rewind;
  }

  // Reaching here means that the rewind point was not found.

  if (break_anywhere_if_overflow_ && !override_break_anywhere_) {
    override_break_anywhere_ = true;
    break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
    Rewind(line_info, 0);
    return LineBreakState::kContinue;
  }

  // Let this line overflow.
  // If there was a break opporunity, the overflow should stop there.
  if (break_before) {
    Rewind(line_info, break_before);
    return LineBreakState::kTrailing;
  }

  return LineBreakState::kTrailing;
}

void NGLineBreaker::Rewind(NGLineInfo* line_info, unsigned new_end) {
  NGInlineItemResults* item_results = &line_info->Results();
  DCHECK_LT(new_end, item_results->size());

  // TODO(ikilpatrick): Add DCHECK that we never rewind past any floats.

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
  item_results->Shrink(new_end);

  SetLineEndFragment(nullptr, line_info);
  UpdatePosition(line_info->Results());
}

// Returns the LayoutObject at the current index/offset.
// This is to tie generated fragments to the source DOM node/LayoutObject for
// paint invalidations, hit testing, etc.
LayoutObject* NGLineBreaker::CurrentLayoutObject(
    const NGLineInfo& line_info) const {
  const Vector<NGInlineItem>& items =
      node_.Items(line_info.UseFirstLineStyle());
  DCHECK_LE(item_index_, items.size());
  // Find the next item that has LayoutObject. Some items such as bidi controls
  // do not have LayoutObject.
  for (unsigned i = item_index_; i < items.size(); i++) {
    if (LayoutObject* layout_object = items[i].GetLayoutObject())
      return layout_object;
  }
  // Find the last item if there were no LayoutObject afterwards.
  for (unsigned i = item_index_; i--;) {
    if (LayoutObject* layout_object = items[i].GetLayoutObject())
      return layout_object;
  }
  NOTREACHED();
  return nullptr;
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
  scoped_refptr<ShapeResult> shape_result =
      shaper.Shape(&font, line_info->BaseDirection());

  // Truncate the line to (available_width - ellipsis_width) using 'line-break:
  // anywhere'.
  unsigned saved_item_index = item_index_;
  unsigned saved_offset = offset_;
  override_break_anywhere_ = true;
  break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
  HandleOverflow(line_info,
                 line_.AvailableWidth() - shape_result->SnappedWidth());

  // Find the LayoutObject this ellpsis is tied to.
  LayoutObject* layout_object = CurrentLayoutObject(*line_info);

  // Restore item_index/offset to before HandleOverflow().
  item_index_ = saved_item_index;
  offset_ = saved_offset;

  // Ellipsis should not have text decorations. Reset if it's set.
  scoped_refptr<const ComputedStyle> style = &line_info->LineStyle();
  if (style->TextDecorationsInEffect() != TextDecoration::kNone) {
    scoped_refptr<ComputedStyle> ellipsis_style =
        ComputedStyle::CreateAnonymousStyleWithDisplay(*style,
                                                       EDisplay::kInline);
    ellipsis_style->ResetTextDecoration();
    ellipsis_style->ClearAppliedTextDecorations();
    style = std::move(ellipsis_style);
  }

  // The ellipsis should appear at the logical end of the line.
  // This is stored seprately from other results so that it can be appended
  // after bidi reorder.
  NGTextFragmentBuilder builder(node_, constraint_space_.GetWritingMode());
  builder.SetText(layout_object, ellipsis, style, std::move(shape_result));
  SetLineEndFragment(builder.ToTextFragment(), line_info);
}

void NGLineBreaker::SetCurrentStyle(const ComputedStyle& style) {
  current_style_ = &style;

  auto_wrap_ = style.AutoWrap();

  if (auto_wrap_) {
    break_iterator_.SetLocale(style.LocaleForLineBreakIterator());

    if (UNLIKELY(override_break_anywhere_)) {
      break_iterator_.SetBreakType(LineBreakType::kBreakCharacter);
    } else {
      switch (style.WordBreak()) {
        case EWordBreak::kNormal:
          break_anywhere_if_overflow_ =
              style.OverflowWrap() == EOverflowWrap::kBreakWord;
          break_iterator_.SetBreakType(LineBreakType::kNormal);
          break;
        case EWordBreak::kBreakAll:
          break_anywhere_if_overflow_ = false;
          break_iterator_.SetBreakType(LineBreakType::kBreakAll);
          break;
        case EWordBreak::kBreakWord:
          break_anywhere_if_overflow_ = true;
          break_iterator_.SetBreakType(LineBreakType::kNormal);
          break;
        case EWordBreak::kKeepAll:
          break_anywhere_if_overflow_ = false;
          break_iterator_.SetBreakType(LineBreakType::kKeepAll);
          break;
      }
    }

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

scoped_refptr<NGInlineBreakToken> NGLineBreaker::CreateBreakToken(
    std::unique_ptr<const NGInlineLayoutStateStack> state_stack) const {
  const Vector<NGInlineItem>& items = node_.Items();
  if (item_index_ >= items.size())
    return NGInlineBreakToken::Create(node_);
  return NGInlineBreakToken::Create(node_, current_style_.get(), item_index_,
                                    offset_, line_.is_after_forced_break,
                                    std::move(state_stack));
}

}  // namespace blink
