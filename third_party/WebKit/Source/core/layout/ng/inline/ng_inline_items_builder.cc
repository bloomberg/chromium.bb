// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_items_builder.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {

template <typename OffsetMappingBuilder>
NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::~NGInlineItemsBuilderTemplate() {
  DCHECK_EQ(0u, exits_.size());
  DCHECK_EQ(text_.length(), items_->IsEmpty() ? 0 : items_->back().EndOffset());
}

template <typename OffsetMappingBuilder>
String NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::ToString() {
  // Segment Break Transformation Rules[1] defines to keep trailing new lines,
  // but it will be removed in Phase II[2]. We prefer not to add trailing new
  // lines and collapsible spaces in Phase I.
  // [1] https://drafts.csswg.org/css-text-3/#line-break-transform
  // [2] https://drafts.csswg.org/css-text-3/#white-space-phase-2
  RemoveTrailingCollapsibleSpaceIfExists();

  return text_.ToString();
}

// Determine "Ambiguous" East Asian Width is Wide or Narrow.
// Unicode East Asian Width
// http://unicode.org/reports/tr11/
static bool IsAmbiguosEastAsianWidthWide(const ComputedStyle* style) {
  UScriptCode script = style->GetFontDescription().GetScript();
  return script == USCRIPT_KATAKANA_OR_HIRAGANA ||
         script == USCRIPT_SIMPLIFIED_HAN || script == USCRIPT_TRADITIONAL_HAN;
}

// Determine if a character has "Wide" East Asian Width.
static bool IsEastAsianWidthWide(UChar32 c, const ComputedStyle* style) {
  UEastAsianWidth eaw = static_cast<UEastAsianWidth>(
      u_getIntPropertyValue(c, UCHAR_EAST_ASIAN_WIDTH));
  return eaw == U_EA_WIDE || eaw == U_EA_FULLWIDTH || eaw == U_EA_HALFWIDTH ||
         (eaw == U_EA_AMBIGUOUS && style &&
          IsAmbiguosEastAsianWidthWide(style));
}

// Determine whether a newline should be removed or not.
// CSS Text, Segment Break Transformation Rules
// https://drafts.csswg.org/css-text-3/#line-break-transform
static bool ShouldRemoveNewlineSlow(const StringBuilder& before,
                                    const ComputedStyle* before_style,
                                    const String& after,
                                    unsigned after_index,
                                    const ComputedStyle* after_style) {
  // Remove if either before/after the newline is zeroWidthSpaceCharacter.
  UChar32 last = 0;
  DCHECK(!before.IsEmpty());
  DCHECK_EQ(before[before.length() - 1], ' ');
  if (before.length() >= 2) {
    last = before[before.length() - 2];
    if (last == kZeroWidthSpaceCharacter)
      return true;
  }
  UChar32 next = 0;
  if (after_index < after.length()) {
    next = after[after_index];
    if (next == kZeroWidthSpaceCharacter)
      return true;
  }

  // Logic below this point requires both before and after be 16 bits.
  if (before.Is8Bit() || after.Is8Bit())
    return false;

  // Remove if East Asian Widths of both before/after the newline are Wide.
  if (U16_IS_TRAIL(last) && before.length() >= 2) {
    UChar last_last = before[before.length() - 2];
    if (U16_IS_LEAD(last_last))
      last = U16_GET_SUPPLEMENTARY(last_last, last);
  }
  if (IsEastAsianWidthWide(last, before_style)) {
    if (U16_IS_LEAD(next) && after_index + 1 < after.length()) {
      UChar next_next = after[after_index + 1];
      if (U16_IS_TRAIL(next_next))
        next = U16_GET_SUPPLEMENTARY(next, next_next);
    }
    if (IsEastAsianWidthWide(next, after_style))
      return true;
  }

  return false;
}

static bool ShouldRemoveNewline(const StringBuilder& before,
                                const ComputedStyle* before_style,
                                const String& after,
                                unsigned after_index,
                                const ComputedStyle* after_style) {
  // All characters before/after removable newline are 16 bits.
  return (!before.Is8Bit() || !after.Is8Bit()) &&
         ShouldRemoveNewlineSlow(before, before_style, after, after_index,
                                 after_style);
}

static void AppendItem(Vector<NGInlineItem>* items,
                       NGInlineItem::NGInlineItemType type,
                       unsigned start,
                       unsigned end,
                       const ComputedStyle* style = nullptr,
                       LayoutObject* layout_object = nullptr) {
  DCHECK(items->IsEmpty() || items->back().EndOffset() == start);
  items->push_back(NGInlineItem(type, start, end, style, layout_object));
}

static inline bool IsCollapsibleSpace(UChar c) {
  return c == kSpaceCharacter || c == kTabulationCharacter ||
         c == kNewlineCharacter;
}

// Characters needing a separate control item than other text items.
// It makes the line breaker easier to handle.
static inline bool IsControlItemCharacter(UChar c) {
  return c == kTabulationCharacter || c == kNewlineCharacter;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Append(
    const String& string,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  if (string.IsEmpty())
    return;
  text_.ReserveCapacity(string.length());

  EWhiteSpace whitespace = style->WhiteSpace();
  if (!ComputedStyle::CollapseWhiteSpace(whitespace))
    return AppendWithoutWhiteSpaceCollapsing(string, style, layout_object);
  if (ComputedStyle::PreserveNewline(whitespace) && !is_svgtext_)
    return AppendWithPreservingNewlines(string, style, layout_object);

  AppendWithWhiteSpaceCollapsing(string, 0, string.length(), style,
                                 layout_object);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::
    AppendWithWhiteSpaceCollapsing(const String& string,
                                   unsigned start,
                                   unsigned end,
                                   const ComputedStyle* style,
                                   LayoutObject* layout_object) {
  unsigned start_offset = text_.length();
  for (unsigned i = start; i < end;) {
    UChar c = string[i];
    if (c == kNewlineCharacter) {
      // LayoutBR does not set preserve_newline, but should be preserved.
      if (!i && end == 1 && layout_object && layout_object->IsBR()) {
        AppendForcedBreak(style, layout_object);
        return;
      }

      if (last_collapsible_space_ == CollapsibleSpace::kNone)
        text_.Append(kSpaceCharacter);
      last_collapsible_space_ = CollapsibleSpace::kNewline;
      i++;
      continue;
    }

    if (c == kSpaceCharacter || c == kTabulationCharacter) {
      if (last_collapsible_space_ == CollapsibleSpace::kNone) {
        text_.Append(kSpaceCharacter);
        last_collapsible_space_ = CollapsibleSpace::kSpace;
      }
      i++;
      continue;
    }

    if (last_collapsible_space_ == CollapsibleSpace::kNewline) {
      RemoveTrailingCollapsibleNewlineIfNeeded(string, i, style);
      start_offset = std::min(start_offset, text_.length());
    }

    size_t end_of_non_space = string.Find(IsCollapsibleSpace, i + 1);
    if (end_of_non_space == kNotFound)
      end_of_non_space = string.length();
    text_.Append(string, i, end_of_non_space - i);
    i = end_of_non_space;
    last_collapsible_space_ = CollapsibleSpace::kNone;
  }

  if (text_.length() > start_offset) {
    AppendItem(items_, NGInlineItem::kText, start_offset, text_.length(), style,
               layout_object);
  }
}

// Even when without whitespace collapsing, control characters (newlines and
// tabs) are in their own control items to make the line breaker easier.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::
    AppendWithoutWhiteSpaceCollapsing(const String& string,
                                      const ComputedStyle* style,
                                      LayoutObject* layout_object) {
  for (unsigned start = 0; start < string.length();) {
    UChar c = string[start];
    if (IsControlItemCharacter(c)) {
      Append(NGInlineItem::kControl, c, style, layout_object);
      start++;
      continue;
    }

    size_t end = string.Find(IsControlItemCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    unsigned start_offset = text_.length();
    text_.Append(string, start, end - start);
    AppendItem(items_, NGInlineItem::kText, start_offset, text_.length(), style,
               layout_object);
    start = end;
  }

  last_collapsible_space_ = CollapsibleSpace::kNone;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::
    AppendWithPreservingNewlines(const String& string,
                                 const ComputedStyle* style,
                                 LayoutObject* layout_object) {
  for (unsigned start = 0; start < string.length();) {
    if (string[start] == kNewlineCharacter) {
      AppendForcedBreak(style, layout_object);
      start++;
      continue;
    }

    size_t end = string.find(kNewlineCharacter, start + 1);
    if (end == kNotFound)
      end = string.length();
    AppendWithWhiteSpaceCollapsing(string, start, end, style, layout_object);
    start = end;
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendForcedBreak(
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  // Remove collapsible spaces immediately before a preserved newline.
  RemoveTrailingCollapsibleSpaceIfExists();

  Append(NGInlineItem::kControl, kNewlineCharacter, style, layout_object);

  // Remove collapsible spaces immediately after a preserved newline.
  last_collapsible_space_ = CollapsibleSpace::kSpace;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Append(
    NGInlineItem::NGInlineItemType type,
    UChar character,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  DCHECK_NE(character, kSpaceCharacter);
  DCHECK_NE(character, kZeroWidthSpaceCharacter);

  text_.Append(character);
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset - 1, end_offset, style, layout_object);
  last_collapsible_space_ = CollapsibleSpace::kNone;
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendOpaque(
    NGInlineItem::NGInlineItemType type,
    UChar character) {
  text_.Append(character);
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset - 1, end_offset, nullptr, nullptr);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendOpaque(
    NGInlineItem::NGInlineItemType type,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset, end_offset, style, layout_object);
}

// Removes the collapsible newline at the end of |text_| if exists and the
// removal conditions met.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::
    RemoveTrailingCollapsibleNewlineIfNeeded(const String& after,
                                             unsigned after_index,
                                             const ComputedStyle* after_style) {
  DCHECK_EQ(last_collapsible_space_, CollapsibleSpace::kNewline);

  if (text_.IsEmpty() || text_[text_.length() - 1] != kSpaceCharacter)
    return;

  const ComputedStyle* before_style = after_style;
  if (!items_->IsEmpty()) {
    NGInlineItem& item = items_->back();
    if (text_.length() < item.EndOffset() + 2)
      before_style = item.Style();
  }

  if (ShouldRemoveNewline(text_, before_style, after, after_index, after_style))
    RemoveTrailingCollapsibleSpace(text_.length() - 1);
}

// Removes the collapsible space at the end of |text_| if exists.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::RemoveTrailingCollapsibleSpaceIfExists() {
  if (last_collapsible_space_ == CollapsibleSpace::kNone || text_.IsEmpty())
    return;

  // Look for the last space character since characters that are opaque to
  // whitespace collapsing may be appended.
  for (unsigned i = text_.length(); i;) {
    UChar ch = text_[--i];
    if (ch == kSpaceCharacter) {
      RemoveTrailingCollapsibleSpace(i);
      return;
    }

    // AppendForcedBreak sets CollapsibleSpace::kSpace to ignore leading
    // spaces. In this case, the trailing collapsible space does not exist.
    if (ch == kNewlineCharacter)
      return;
  }
  NOTREACHED();
}

// Removes the collapsible space at the specified index.
template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<
    OffsetMappingBuilder>::RemoveTrailingCollapsibleSpace(unsigned index) {
  DCHECK_NE(last_collapsible_space_, CollapsibleSpace::kNone);
  DCHECK(!text_.IsEmpty());
  DCHECK_EQ(text_[index], kSpaceCharacter);

  text_.erase(index);
  last_collapsible_space_ = CollapsibleSpace::kNone;

  // Adjust items if the removed space is already included.
  for (unsigned i = items_->size(); i > 0;) {
    NGInlineItem& item = (*items_)[--i];
    if (index >= item.EndOffset())
      return;
    if (item.StartOffset() <= index) {
      if (item.Length() == 1) {
        DCHECK_EQ(item.StartOffset(), index);
        DCHECK_EQ(item.Type(), NGInlineItem::kText);
        items_->erase(i);
      } else {
        item.SetEndOffset(item.EndOffset() - 1);
      }
      return;
    }

    // Trailing spaces can be removed across non-character items.
    // Adjust their offsets if after the removed index.
    item.SetOffset(item.StartOffset() - 1, item.EndOffset() - 1);
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::AppendBidiControl(
    const ComputedStyle* style,
    UChar ltr,
    UChar rtl) {
  AppendOpaque(NGInlineItem::kBidiControl,
               IsLtr(style->Direction()) ? ltr : rtl);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterBlock(
    const ComputedStyle* style) {
  // Handle bidi-override on the block itself.
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
      AppendBidiControl(style, kLeftToRightOverrideCharacter,
                        kRightToLeftOverrideCharacter);
      Enter(nullptr, kPopDirectionalFormattingCharacter);
      break;
    case UnicodeBidi::kPlaintext:
      // Plaintext is handled as the paragraph level by
      // NGBidiParagraph::SetParagraph().
      has_bidi_controls_ = true;
      break;
  }
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::EnterInline(
    LayoutObject* node) {
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const ComputedStyle* style = node->Style();
  switch (style->GetUnicodeBidi()) {
    case UnicodeBidi::kNormal:
      break;
    case UnicodeBidi::kEmbed:
      AppendBidiControl(style, kLeftToRightEmbedCharacter,
                        kRightToLeftEmbedCharacter);
      Enter(node, kPopDirectionalFormattingCharacter);
      break;
    case UnicodeBidi::kBidiOverride:
      AppendBidiControl(style, kLeftToRightOverrideCharacter,
                        kRightToLeftOverrideCharacter);
      Enter(node, kPopDirectionalFormattingCharacter);
      break;
    case UnicodeBidi::kIsolate:
      AppendBidiControl(style, kLeftToRightIsolateCharacter,
                        kRightToLeftIsolateCharacter);
      Enter(node, kPopDirectionalIsolateCharacter);
      break;
    case UnicodeBidi::kPlaintext:
      AppendOpaque(NGInlineItem::kBidiControl, kFirstStrongIsolateCharacter);
      Enter(node, kPopDirectionalIsolateCharacter);
      break;
    case UnicodeBidi::kIsolateOverride:
      AppendOpaque(NGInlineItem::kBidiControl, kFirstStrongIsolateCharacter);
      AppendBidiControl(style, kLeftToRightOverrideCharacter,
                        kRightToLeftOverrideCharacter);
      Enter(node, kPopDirectionalIsolateCharacter);
      Enter(node, kPopDirectionalFormattingCharacter);
      break;
  }

  AppendOpaque(NGInlineItem::kOpenTag, style, node);
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Enter(
    LayoutObject* node,
    UChar character_to_exit) {
  exits_.push_back(OnExitNode{node, character_to_exit});
  has_bidi_controls_ = true;
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
}

template <typename OffsetMappingBuilder>
void NGInlineItemsBuilderTemplate<OffsetMappingBuilder>::Exit(
    LayoutObject* node) {
  while (!exits_.IsEmpty() && exits_.back().node == node) {
    AppendOpaque(NGInlineItem::kBidiControl, exits_.back().character);
    exits_.pop_back();
  }
}

template class CORE_TEMPLATE_EXPORT
    NGInlineItemsBuilderTemplate<EmptyOffsetMappingBuilder>;

}  // namespace blink
