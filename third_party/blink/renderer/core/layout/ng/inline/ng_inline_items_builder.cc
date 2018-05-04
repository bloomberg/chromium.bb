// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_builder.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

template <typename OffsetMappingBuilder>
NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::~NGInlineItemsBuilderTemplate() {
  DCHECK_EQ(0u, bidi_context_.size());
  DCHECK_EQ(text_.length(), items_->IsEmpty() ? 0 : items_->back().EndOffset());
}

template <typename OffsetMappingBuilder>
String NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::ToString() {
  // Segment Break Transformation Rules[1] defines to keep trailing new lines in
  // Phase I, but to remove after line break, in Phase II[2]. Although the spec
  // defines so, trailing collapsible spaces at the end of an inline formatting
  // context will be removed in Phase II and that removing here makes no
  // differences.
  //
  // However, doing so reduces the opportunities to re-use NGInlineItem a lot in
  // appending scenario, which is quite common. In order to re-use NGInlineItem
  // as much as posssible, trailing spaces are removed in Phase II, exactly as
  // defined in the spec.
  //
  // [1] https://drafts.csswg.org/css-text-3/#line-break-transform
  // [2] https://drafts.csswg.org/css-text-3/#white-space-phase-2
  return text_.ToString();
}

template <>
String NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::ToString() {
  // While trailing collapsible space is kept as above, NGOffsetMappingBuilder
  // assumes NGLineBreaker does not remove it. For now, remove only for
  // NGOffsetMappingBuilder.
  // TODO(kojii): Consider NGOffsetMappingBuilder to support NGLineBreaker to
  // remove trailing spaces.
  RemoveTrailingCollapsibleSpaceIfExists();

  return text_.ToString();
}

namespace {
// Determine "Ambiguous" East Asian Width is Wide or Narrow.
// Unicode East Asian Width
// http://unicode.org/reports/tr11/
bool IsAmbiguosEastAsianWidthWide(const ComputedStyle* style) {
  UScriptCode script = style->GetFontDescription().GetScript();
  return script == USCRIPT_KATAKANA_OR_HIRAGANA ||
         script == USCRIPT_SIMPLIFIED_HAN || script == USCRIPT_TRADITIONAL_HAN;
}

// Determine if a character has "Wide" East Asian Width.
bool IsEastAsianWidthWide(UChar32 c, const ComputedStyle* style) {
  UEastAsianWidth eaw = static_cast<UEastAsianWidth>(
      u_getIntPropertyValue(c, UCHAR_EAST_ASIAN_WIDTH));
  return eaw == U_EA_WIDE || eaw == U_EA_FULLWIDTH || eaw == U_EA_HALFWIDTH ||
         (eaw == U_EA_AMBIGUOUS && style &&
          IsAmbiguosEastAsianWidthWide(style));
}

// Determine whether a newline should be removed or not.
// CSS Text, Segment Break Transformation Rules
// https://drafts.csswg.org/css-text-3/#line-break-transform
bool ShouldRemoveNewlineSlow(const StringBuilder& before,
                             unsigned space_index,
                             const ComputedStyle* before_style,
                             const StringView& after,
                             const ComputedStyle* after_style) {
  // Remove if either before/after the newline is zeroWidthSpaceCharacter.
  UChar32 last = 0;
  DCHECK(space_index == before.length() ||
         (space_index < before.length() && before[space_index] == ' '));
  if (space_index) {
    last = before[space_index - 1];
    if (last == kZeroWidthSpaceCharacter)
      return true;
  }
  UChar32 next = 0;
  if (!after.IsEmpty()) {
    next = after[0];
    if (next == kZeroWidthSpaceCharacter)
      return true;
  }

  // Logic below this point requires both before and after be 16 bits.
  if (before.Is8Bit() || after.Is8Bit())
    return false;

  // Remove if East Asian Widths of both before/after the newline are Wide.
  if (U16_IS_TRAIL(last) && space_index >= 2) {
    UChar last_last = before[space_index - 2];
    if (U16_IS_LEAD(last_last))
      last = U16_GET_SUPPLEMENTARY(last_last, last);
  }
  if (IsEastAsianWidthWide(last, before_style)) {
    if (U16_IS_LEAD(next) && after.length() > 1) {
      UChar next_next = after[1];
      if (U16_IS_TRAIL(next_next))
        next = U16_GET_SUPPLEMENTARY(next, next_next);
    }
    if (IsEastAsianWidthWide(next, after_style))
      return true;
  }

  return false;
}

bool ShouldRemoveNewline(const StringBuilder& before,
                         unsigned space_index,
                         const ComputedStyle* before_style,
                         const StringView& after,
                         const ComputedStyle* after_style) {
  // All characters before/after removable newline are 16 bits.
  return (!before.Is8Bit() || !after.Is8Bit()) &&
         ShouldRemoveNewlineSlow(before, space_index, before_style, after,
                                 after_style);
}

void AppendItem(Vector<NGInlineItem>* items,
                NGInlineItem::NGInlineItemType type,
                unsigned start,
                unsigned end,
                const ComputedStyle* style = nullptr,
                LayoutObject* layout_object = nullptr,
                bool end_may_collapse = false) {
  items->push_back(
      NGInlineItem(type, start, end, style, layout_object, end_may_collapse));
}

inline bool ShouldIgnore(UChar c) {
  // Ignore carriage return and form feed.
  // https://drafts.csswg.org/css-text-3/#white-space-processing
  // https://github.com/w3c/csswg-drafts/issues/855
  //
  // Unicode Default_Ignorable is not included because we need some of them
  // in the line breaker (e.g., SOFT HYPHEN.) HarfBuzz ignores them while
  // shaping.
  return c == kCarriageReturnCharacter || c == kFormFeedCharacter;
}

inline bool IsCollapsibleSpace(UChar c) {
  return c == kSpaceCharacter || c == kNewlineCharacter ||
         c == kTabulationCharacter || c == kCarriageReturnCharacter;
}

// Characters needing a separate control item than other text items.
// It makes the line breaker easier to handle.
inline bool IsControlItemCharacter(UChar c) {
  return c == kNewlineCharacter || c == kTabulationCharacter ||
         // Include ignorable character here to avoids shaping/rendering
         // these glyphs, and to help the line breaker to ignore them.
         ShouldIgnore(c);
}

// Find the end of the collapsible spaces.
// Returns whether this space run contains a newline or not, because it changes
// the collapsing behavior.
inline bool MoveToEndOfCollapsibleSpaces(const StringView& string,
                                         unsigned* offset,
                                         UChar* c) {
  DCHECK_EQ(*c, string[*offset]);
  DCHECK(IsCollapsibleSpace(*c));
  bool space_run_has_newline = *c == kNewlineCharacter;
  for ((*offset)++; *offset < string.length(); (*offset)++) {
    *c = string[*offset];
    space_run_has_newline |= *c == kNewlineCharacter;
    if (!IsCollapsibleSpace(*c))
      break;
  }
  return space_run_has_newline;
}

// Find the last item to compute collapsing with. Opaque items such as
// open/close or bidi controls are ignored.
// Returns nullptr if there were no previous items.
NGInlineItem* LastItemToCollapseWith(Vector<NGInlineItem>* items) {
  for (auto it = items->rbegin(); it != items->rend(); it++) {
    NGInlineItem& item = *it;
    if (item.EndCollapseType() != NGInlineItem::kOpaqueToCollapsing)
      return &item;
  }
  return nullptr;
}

inline bool MayCollapseWithLast(const NGInlineItem* item) {
  return item && item->EndMayCollapse();
}

}  // anonymous namespace

template <typename OffsetMappingBuilder>
bool NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Append(
    const String& original_string,
    LayoutObject* layout_object,
    const Vector<NGInlineItem*>& items) {
  // Don't reuse existing items if they might be affected by whitespace
  // collapsing.
  // TODO(layout-dev): This could likely be optimized further.
  // TODO(layout-dev): Handle cases where the old items are not consecutive.
  if (MayCollapseWithLast(LastItemToCollapseWith(items_)) ||
      IsCollapsibleSpace(original_string[items[0]->StartOffset()]))
    return false;

  for (const NGInlineItem* item : items) {
    unsigned start = text_.length();
    text_.Append(original_string, item->StartOffset(), item->Length());

    // If the item's position within the container remains unchanged the item
    // itself may be reused.
    if (item->StartOffset() == start) {
      items_->push_back(*item);
      is_empty_inline_ &= item->IsEmptyItem();
      continue;
    }

    // If the position has shifted the item and the shape result needs to be
    // adjusted to reflect the new start and end offsets.
    unsigned end = start + item->Length();
    DCHECK(item->TextShapeResult());
    NGInlineItem adjusted_item(
        *item, start, end, item->TextShapeResult()->CopyAdjustedOffset(start));

    DCHECK(adjusted_item.TextShapeResult());
    DCHECK_EQ(start, adjusted_item.StartOffset());
    DCHECK_EQ(start, adjusted_item.TextShapeResult()->StartIndexForResult());
    DCHECK_EQ(end, adjusted_item.EndOffset());
    DCHECK_EQ(end, adjusted_item.TextShapeResult()->EndIndexForResult());
    DCHECK_EQ(item->IsEmptyItem(), adjusted_item.IsEmptyItem());

    items_->push_back(adjusted_item);
    is_empty_inline_ &= adjusted_item.IsEmptyItem();
  }
  return true;
}

template <>
bool NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>::Append(
    const String&,
    LayoutObject*,
    const Vector<NGInlineItem*>&) {
  NOTREACHED();
  return false;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Append(
    const String& string,
    const ComputedStyle* style,
    LayoutText* layout_object) {
  if (string.IsEmpty())
    return;
  text_.ReserveCapacity(string.length());

  typename OffsetMappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                       layout_object);

  EWhiteSpace whitespace = style->WhiteSpace();
  bool is_svg_text = layout_object && layout_object->IsSVGInlineText();

  if (!ComputedStyle::CollapseWhiteSpace(whitespace))
    AppendPreserveWhitespace(string, style, layout_object);
  else if (ComputedStyle::PreserveNewline(whitespace) && !is_svg_text)
    AppendPreserveNewline(string, style, layout_object);
  else
    AppendCollapseWhitespace(string, style, layout_object);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::AppendCollapseWhitespace(const StringView string,
                                                    const ComputedStyle* style,
                                                    LayoutText* layout_object) {
  DCHECK(!string.IsEmpty());

  // This algorithm segments the input string at the collapsible space, and
  // process collapsible space run and non-space run alternately.

  // The first run, regardless it is a collapsible space run or not, is special
  // that it can interact with the last item. Depends on the end of the last
  // item, it may either change collapsing behavior to collapse the leading
  // spaces of this item entirely, or remove the trailing spaces of the last
  // item.

  // Due to this difference, this algorithm process the first run first, then
  // loop through the rest of runs.

  unsigned start_offset;
  NGInlineItem::NGCollapseType end_collapse = NGInlineItem::kNotCollapsible;
  unsigned i = 0;
  UChar c = string[i];
  if (IsCollapsibleSpace(c)) {
    // Find the end of the collapsible space run.
    bool space_run_has_newline = MoveToEndOfCollapsibleSpaces(string, &i, &c);

    // LayoutBR does not set preserve_newline, but should be preserved.
    if (UNLIKELY(space_run_has_newline && string.length() == 1 &&
                 layout_object && layout_object->IsBR())) {
      AppendForcedBreakCollapseWhitespace(style, layout_object);
      return;
    }

    // Check the last item this space run may be collapsed with.
    bool insert_space;
    if (NGInlineItem* item = LastItemToCollapseWith(items_)) {
      if (item->EndCollapseType() == NGInlineItem::kNotCollapsible) {
        // The last item does not end with a collapsible space.
        // Insert a space to represent this space run.
        insert_space = true;
      } else {
        // The last item ends with a collapsible space this run should collapse
        // to. Collapse the entire space run in this item.
        DCHECK(item->EndCollapseType() == NGInlineItem::kCollapsibleSpace ||
               item->EndCollapseType() == NGInlineItem::kCollapsibleNewline);
        insert_space = false;

        // If the space run either in this item or in the last item contains a
        // newline, apply segment break rules. This may result in removal of
        // the space in the last item.
        if ((space_run_has_newline ||
             item->EndCollapseType() == NGInlineItem::kCollapsibleNewline) &&
            item->Type() == NGInlineItem::kText &&
            ShouldRemoveNewline(text_, item->EndOffset() - 1, item->Style(),
                                StringView(string, i), style)) {
          RemoveTrailingCollapsibleSpace(item);
          space_run_has_newline = false;
        } else if (!item->Style()->AutoWrap() && style->AutoWrap()) {
          // Otherwise, remove the space run entirely, collapsing to the space
          // in the last item.

          // There is a special case to generate a break opportunity though.
          // Spec-wise, collapsed spaces are "zero advance width, invisible,
          // but retains its soft wrap opportunity".
          // https://drafts.csswg.org/css-text-3/#collapse
          // In most cases, this is not needed and that collapsed spaces are
          // removed entirely. However, when the first collapsible space is
          // 'nowrap', and the following collapsed space is 'wrap', the
          // collapsed space needs to create a break opportunity.
          typename OffsetMappingBuilder::SourceNodeScope scope(
              &mapping_builder_, nullptr);
          AppendBreakOpportunity(style, nullptr);
        }
      }
    } else {
      // This space is at the beginning of the paragraph. Remove leading spaces
      // as CSS requires.
      insert_space = false;
    }

    // If this space run contains a newline, apply segment break rules.
    if (space_run_has_newline &&
        ShouldRemoveNewline(text_, text_.length(), style, StringView(string, i),
                            style)) {
      insert_space = space_run_has_newline = false;
    }

    // Done computing the interaction with the last item. Start appending.
    start_offset = text_.length();

    DCHECK(i);
    unsigned collapsed_length = i;
    if (insert_space) {
      text_.Append(kSpaceCharacter);
      mapping_builder_.AppendIdentityMapping(1);
      collapsed_length--;
    }
    if (collapsed_length)
      mapping_builder_.AppendCollapsedMapping(collapsed_length);

    // If this space run is at the end of this item, keep whether the
    // collapsible space run has a newline or not in the item.
    if (i == string.length()) {
      end_collapse = space_run_has_newline ? NGInlineItem::kCollapsibleNewline
                                           : NGInlineItem::kCollapsibleSpace;
    }
  } else {
    // If the last item ended with a collapsible space run with segment breaks,
    // apply segment break rules. This may result in removal of the space in the
    // last item.
    if (NGInlineItem* item = LastItemToCollapseWith(items_)) {
      if (item->EndCollapseType() == NGInlineItem::kCollapsibleNewline &&
          ShouldRemoveNewline(text_, item->EndOffset() - 1, item->Style(),
                              string, style)) {
        RemoveTrailingCollapsibleSpace(item);
      }
    }

    start_offset = text_.length();
  }

  // The first run is done. Loop through the rest of runs.
  if (i < string.length()) {
    while (true) {
      // Append the non-space text until we find a collapsible space.
      // |string[i]| is guaranteed not to be a space.
      DCHECK(!IsCollapsibleSpace(string[i]));
      unsigned start_of_non_space = i;
      for (i++; i < string.length(); i++) {
        c = string[i];
        if (IsCollapsibleSpace(c))
          break;
      }
      text_.Append(string, start_of_non_space, i - start_of_non_space);
      mapping_builder_.AppendIdentityMapping(i - start_of_non_space);

      if (i == string.length())
        break;

      // Process a collapsible space run. First, find the end of the run.
      DCHECK_EQ(c, string[i]);
      DCHECK(IsCollapsibleSpace(c));
      unsigned start_of_spaces = i;
      bool space_run_has_newline = MoveToEndOfCollapsibleSpaces(string, &i, &c);

      // Because leading spaces are handled before this loop, no need to check
      // cross-item collapsing.
      DCHECK(start_of_spaces);

      // If this space run contains a newline, apply segment break rules.
      if (space_run_has_newline &&
          ShouldRemoveNewline(text_, text_.length(), style,
                              StringView(string, i), style)) {
        space_run_has_newline = false;
      } else {
        // Otherwise, or if the segment break rules did not remove the run,
        // append a space.
        text_.Append(kSpaceCharacter);
        mapping_builder_.AppendIdentityMapping(1);
        start_of_spaces++;
      }

      if (i != start_of_spaces)
        mapping_builder_.AppendCollapsedMapping(i - start_of_spaces);

      // If this space run is at the end of this item, keep whether the
      // collapsible space run has a newline or not in the item.
      if (i == string.length()) {
        end_collapse = space_run_has_newline ? NGInlineItem::kCollapsibleNewline
                                             : NGInlineItem::kCollapsibleSpace;
        break;
      }
    }
  }

  if (text_.length() > start_offset) {
    AppendItem(items_, NGInlineItem::kText, start_offset, text_.length(), style,
               layout_object, end_collapse != NGInlineItem::kNotCollapsible);
    NGInlineItem& item = items_->back();
    item.SetEndCollapseType(end_collapse);
    is_empty_inline_ &= item.IsEmptyItem();
  }
}

// Even when without whitespace collapsing, control characters (newlines and
// tabs) are in their own control items to make the line breaker easier.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::AppendPreserveWhitespace(const String& string,
                                                    const ComputedStyle* style,
                                                    LayoutText* layout_object) {
  for (unsigned start = 0; start < string.length();) {
    UChar c = string[start];
    if (IsControlItemCharacter(c)) {
      if (c != kNewlineCharacter)
        Append(NGInlineItem::kControl, c, style, layout_object);
      else
        AppendForcedBreak(style, layout_object);
      start++;
      continue;
    }

    size_t end = string.Find(IsControlItemCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    unsigned start_offset = text_.length();
    text_.Append(string, start, end - start);
    mapping_builder_.AppendIdentityMapping(end - start);
    AppendItem(items_, NGInlineItem::kText, start_offset, text_.length(), style,
               layout_object);

    is_empty_inline_ &= items_->back().IsEmptyItem();
    start = end;
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendPreserveNewline(
    const String& string,
    const ComputedStyle* style,
    LayoutText* layout_object) {
  for (unsigned start = 0; start < string.length();) {
    if (string[start] == kNewlineCharacter) {
      AppendForcedBreakCollapseWhitespace(style, layout_object);
      start++;
      continue;
    }

    size_t end = string.find(kNewlineCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    DCHECK_GE(end, start);
    AppendCollapseWhitespace(StringView(string, start, end - start), style,
                             layout_object);
    start = end;
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendForcedBreak(
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  // At the forced break, add bidi controls to pop all contexts.
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-embedding-breaks
  if (!bidi_context_.IsEmpty()) {
    typename OffsetMappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                         nullptr);
    for (auto it = bidi_context_.rbegin(); it != bidi_context_.rend(); ++it)
      AppendOpaque(NGInlineItem::kBidiControl, it->exit);
  }

  Append(NGInlineItem::kControl, kNewlineCharacter, style, layout_object);

  // A forced break is not a collapsible space, but following collapsible spaces
  // are leading spaces and that they should be collapsed.
  // Pretend that this item ends with a collapsible space, so that following
  // collapsible spaces can be collapsed.
  items_->back().SetEndCollapseType(NGInlineItem::kCollapsibleSpace);

  // Then re-add bidi controls to restore the bidi context.
  if (!bidi_context_.IsEmpty()) {
    typename OffsetMappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                         nullptr);
    for (const auto& bidi : bidi_context_)
      AppendOpaque(NGInlineItem::kBidiControl, bidi.enter);
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::
    AppendForcedBreakCollapseWhitespace(const ComputedStyle* style,
                                        LayoutObject* layout_object) {
  // Remove collapsible spaces immediately before a preserved newline.
  RemoveTrailingCollapsibleSpaceIfExists();

  AppendForcedBreak(style, layout_object);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendBreakOpportunity(
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  AppendOpaque(NGInlineItem::kControl, kZeroWidthSpaceCharacter, style,
               layout_object);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Append(
    NGInlineItem::NGInlineItemType type,
    UChar character,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  DCHECK_NE(character, kSpaceCharacter);

  text_.Append(character);
  mapping_builder_.AppendIdentityMapping(1);
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset - 1, end_offset, style, layout_object);

  is_empty_inline_ &= items_->back().IsEmptyItem();
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendAtomicInline(
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  typename OffsetMappingBuilder::SourceNodeScope scope(&mapping_builder_,
                                                       layout_object);
  Append(NGInlineItem::kAtomicInline, kObjectReplacementCharacter, style,
         layout_object);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendOpaque(
    NGInlineItem::NGInlineItemType type,
    UChar character,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  text_.Append(character);
  mapping_builder_.AppendIdentityMapping(1);
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset - 1, end_offset, style, layout_object);

  NGInlineItem& item = items_->back();
  item.SetEndCollapseType(NGInlineItem::kOpaqueToCollapsing);
  is_empty_inline_ &= item.IsEmptyItem();
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendOpaque(
    NGInlineItem::NGInlineItemType type,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset, end_offset, style, layout_object);

  NGInlineItem& item = items_->back();
  item.SetEndCollapseType(NGInlineItem::kOpaqueToCollapsing);
  is_empty_inline_ &= item.IsEmptyItem();
}

// Removes the collapsible space at the end of |text_| if exists.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::RemoveTrailingCollapsibleSpaceIfExists() {
  if (NGInlineItem* item = LastItemToCollapseWith(items_)) {
    if (item->EndCollapseType() != NGInlineItem::kNotCollapsible)
      RemoveTrailingCollapsibleSpace(item);
  }
}

// Removes the collapsible space at the end of the specified item.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::RemoveTrailingCollapsibleSpace(NGInlineItem* item) {
  DCHECK(item);
  DCHECK_GT(item->Length(), 0u);
  DCHECK(item->EndCollapseType() == NGInlineItem::kCollapsibleSpace ||
         item->EndCollapseType() == NGInlineItem::kCollapsibleNewline);

  // A forced break pretends that it's a collapsible space, see
  // |AppendForcedBreak()|. It should not be removed.
  if (item->Type() == NGInlineItem::kControl)
    return;
  DCHECK_EQ(item->Type(), NGInlineItem::kText);

  unsigned space_offset = item->EndOffset() - 1;
  DCHECK_EQ(text_[space_offset], kSpaceCharacter);
  text_.erase(space_offset);
  mapping_builder_.CollapseTrailingSpace(text_.length() - space_offset);

  // Remove the item if the item has only one space that we're removing.
  if (item->Length() == 1) {
    DCHECK_EQ(item->StartOffset(), space_offset);
    unsigned index = std::distance(items_->begin(), item);
    items_->EraseAt(index);
    if (index == items_->size())
      return;
    // Re-compute |item| because |EraseAt| may have reallocated the buffer.
    item = &(*items_)[index];
  } else {
    item->SetEndOffset(item->EndOffset() - 1);
    item->SetEndCollapseType(NGInlineItem::kNotCollapsible);
    item++;
  }

  // Trailing spaces can be removed across non-character items.
  // Adjust their offsets if after the removed index.
  for (; item != items_->end(); item++) {
    item->SetOffset(item->StartOffset() - 1, item->EndOffset() - 1);
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterBidiContext(
    LayoutObject* node,
    UChar enter,
    UChar exit) {
  AppendOpaque(NGInlineItem::kBidiControl, enter);
  bidi_context_.push_back(BidiContext{node, enter, exit});
  has_bidi_controls_ = true;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterBidiContext(
    LayoutObject* node,
    const ComputedStyle* style,
    UChar ltr_enter,
    UChar rtl_enter,
    UChar exit) {
  EnterBidiContext(node, IsLtr(style->Direction()) ? ltr_enter : rtl_enter,
                   exit);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterBlock(
    const ComputedStyle* style) {
  // Handle bidi-override on the block itself.
  if (style->RtlOrdering() == EOrder::kLogical) {
    switch (style->GetUnicodeBidi()) {
      case UnicodeBidi::kNormal:
      case UnicodeBidi::kEmbed:
      case UnicodeBidi::kIsolate:
        // Isolate and embed values are enforced by default and redundant on the
        // block elements.
        // Direction is handled as the paragraph level by
        // NGBidiParagraph::SetParagraph().
        if (style->Direction() == TextDirection::kRtl)
          has_bidi_controls_ = true;
        break;
      case UnicodeBidi::kBidiOverride:
      case UnicodeBidi::kIsolateOverride:
        EnterBidiContext(nullptr, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kPlaintext:
        // Plaintext is handled as the paragraph level by
        // NGBidiParagraph::SetParagraph().
        has_bidi_controls_ = true;
        break;
    }
  } else {
    DCHECK_EQ(style->RtlOrdering(), EOrder::kVisual);
    EnterBidiContext(nullptr, style, kLeftToRightOverrideCharacter,
                     kRightToLeftOverrideCharacter,
                     kPopDirectionalFormattingCharacter);
  }

  if (style->Display() == EDisplay::kListItem &&
      style->ListStyleType() != EListStyleType::kNone)
    is_empty_inline_ = false;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterInline(
    LayoutObject* node) {
  DCHECK(node);
  mapping_builder_.EnterInline(*node);

  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const ComputedStyle* style = node->Style();
  if (style->RtlOrdering() == EOrder::kLogical) {
    switch (style->GetUnicodeBidi()) {
      case UnicodeBidi::kNormal:
        break;
      case UnicodeBidi::kEmbed:
        EnterBidiContext(node, style, kLeftToRightEmbedCharacter,
                         kRightToLeftEmbedCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kBidiOverride:
        EnterBidiContext(node, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
      case UnicodeBidi::kIsolate:
        EnterBidiContext(node, style, kLeftToRightIsolateCharacter,
                         kRightToLeftIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        break;
      case UnicodeBidi::kPlaintext:
        EnterBidiContext(node, kFirstStrongIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        break;
      case UnicodeBidi::kIsolateOverride:
        EnterBidiContext(node, kFirstStrongIsolateCharacter,
                         kPopDirectionalIsolateCharacter);
        EnterBidiContext(node, style, kLeftToRightOverrideCharacter,
                         kRightToLeftOverrideCharacter,
                         kPopDirectionalFormattingCharacter);
        break;
    }
  }

  AppendOpaque(NGInlineItem::kOpenTag, style, node);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::ExitBlock() {
  Exit(nullptr);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::ExitInline(
    LayoutObject* node) {
  DCHECK(node);

  AppendOpaque(NGInlineItem::kCloseTag, node->Style(), node);

  Exit(node);

  mapping_builder_.ExitInline(*node);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Exit(
    LayoutObject* node) {
  while (!bidi_context_.IsEmpty() && bidi_context_.back().node == node) {
    AppendOpaque(NGInlineItem::kBidiControl, bidi_context_.back().exit);
    bidi_context_.pop_back();
  }
}

template class CORE_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;
template class CORE_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<NGOffsetMappingBuilder>;

}  // namespace blink
