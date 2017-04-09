// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_inline_items_builder.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/style/ComputedStyle.h"

namespace blink {

NGLayoutInlineItemsBuilder::~NGLayoutInlineItemsBuilder() {
  DCHECK_EQ(0u, exits_.size());
  DCHECK_EQ(text_.length(), items_->IsEmpty() ? 0 : items_->back().EndOffset());
}

String NGLayoutInlineItemsBuilder::ToString() {
  // Segment Break Transformation Rules[1] defines to keep trailing new lines,
  // but it will be removed in Phase II[2]. We prefer not to add trailing new
  // lines and collapsible spaces in Phase I.
  // [1] https://drafts.csswg.org/css-text-3/#line-break-transform
  // [2] https://drafts.csswg.org/css-text-3/#white-space-phase-2
  unsigned next_start_offset = text_.length();
  RemoveTrailingCollapsibleSpaceIfExists(&next_start_offset);

  return text_.ToString();
}

// Determine "Ambiguous" East Asian Width is Wide or Narrow.
// Unicode East Asian Width
// http://unicode.org/reports/tr11/
static bool IsAmbiguosEastAsianWidthWide(const ComputedStyle* style) {
  UScriptCode script = style->GetFontDescription().Script();
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

static void AppendItem(Vector<NGLayoutInlineItem>* items,
                       NGLayoutInlineItem::NGLayoutInlineItemType type,
                       unsigned start,
                       unsigned end,
                       const ComputedStyle* style = nullptr,
                       LayoutObject* layout_object = nullptr) {
  DCHECK(items->IsEmpty() || items->back().EndOffset() == start);
  items->push_back(NGLayoutInlineItem(type, start, end, style, layout_object));
}

static inline bool IsCollapsibleSpace(UChar c, bool preserve_newline) {
  return c == kSpaceCharacter || c == kTabulationCharacter ||
         (!preserve_newline && c == kNewlineCharacter);
}

void NGLayoutInlineItemsBuilder::Append(const String& string,
                                        const ComputedStyle* style,
                                        LayoutObject* layout_object) {
  if (string.IsEmpty())
    return;

  EWhiteSpace whitespace = style->WhiteSpace();
  bool preserve_newline =
      ComputedStyle::PreserveNewline(whitespace) && !is_svgtext_;
  bool collapse_whitespace = ComputedStyle::CollapseWhiteSpace(whitespace);
  unsigned start_offset = text_.length();

  if (!collapse_whitespace) {
    text_.Append(string);
    last_collapsible_space_ = CollapsibleSpace::kNone;
  } else {
    text_.ReserveCapacity(string.length());
    for (unsigned i = 0; i < string.length();) {
      UChar c = string[i];
      if (c == kNewlineCharacter) {
        if (preserve_newline) {
          RemoveTrailingCollapsibleSpaceIfExists(&start_offset);
          text_.Append(c);
          // Remove collapsible spaces immediately following a newline.
          last_collapsible_space_ = CollapsibleSpace::kSpace;
          i++;
          continue;
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
        RemoveTrailingCollapsibleNewlineIfNeeded(&start_offset, string, i,
                                                 style);
      }

      unsigned start_of_non_space = i;
      for (i++; i < string.length(); i++) {
        if (IsCollapsibleSpace(string[i], false))
          break;
      }
      text_.Append(string, start_of_non_space, i - start_of_non_space);
      last_collapsible_space_ = CollapsibleSpace::kNone;
    }
  }

  if (text_.length() > start_offset) {
    AppendItem(items_, NGLayoutInlineItem::kText, start_offset, text_.length(),
               style, layout_object);
  }
}

void NGLayoutInlineItemsBuilder::Append(
    NGLayoutInlineItem::NGLayoutInlineItemType type,
    UChar character,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  DCHECK_NE(character, kSpaceCharacter);
  DCHECK_NE(character, kTabulationCharacter);
  DCHECK_NE(character, kNewlineCharacter);
  DCHECK_NE(character, kZeroWidthSpaceCharacter);

  text_.Append(character);
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset - 1, end_offset, style, layout_object);
  last_collapsible_space_ = CollapsibleSpace::kNone;
}

void NGLayoutInlineItemsBuilder::Append(
    NGLayoutInlineItem::NGLayoutInlineItemType type,
    const ComputedStyle* style,
    LayoutObject* layout_object) {
  unsigned end_offset = text_.length();
  AppendItem(items_, type, end_offset, end_offset, style, layout_object);
}

void NGLayoutInlineItemsBuilder::RemoveTrailingCollapsibleNewlineIfNeeded(
    unsigned* next_start_offset,
    const String& after,
    unsigned after_index,
    const ComputedStyle* after_style) {
  DCHECK_EQ(last_collapsible_space_, CollapsibleSpace::kNewline);

  if (text_.IsEmpty() || text_[text_.length() - 1] != kSpaceCharacter)
    return;

  const ComputedStyle* before_style = after_style;
  if (!items_->IsEmpty()) {
    NGLayoutInlineItem& item = items_->back();
    if (text_.length() < item.EndOffset() + 2)
      before_style = item.Style();
  }

  if (ShouldRemoveNewline(text_, before_style, after, after_index, after_style))
    RemoveTrailingCollapsibleSpace(next_start_offset);
}

void NGLayoutInlineItemsBuilder::RemoveTrailingCollapsibleSpaceIfExists(
    unsigned* next_start_offset) {
  if (last_collapsible_space_ != CollapsibleSpace::kNone && !text_.IsEmpty() &&
      text_[text_.length() - 1] == kSpaceCharacter)
    RemoveTrailingCollapsibleSpace(next_start_offset);
}

void NGLayoutInlineItemsBuilder::RemoveTrailingCollapsibleSpace(
    unsigned* next_start_offset) {
  DCHECK_NE(last_collapsible_space_, CollapsibleSpace::kNone);
  DCHECK(!text_.IsEmpty());
  DCHECK_EQ(text_[text_.length() - 1], kSpaceCharacter);

  unsigned new_size = text_.length() - 1;
  text_.Resize(new_size);
  last_collapsible_space_ = CollapsibleSpace::kNone;

  if (*next_start_offset <= new_size)
    return;
  *next_start_offset = new_size;

  // Adjust the last item if the removed space is already appended.
  for (unsigned i = items_->size(); i > 0;) {
    NGLayoutInlineItem& item = (*items_)[--i];
    DCHECK_EQ(item.EndOffset(), new_size + 1);
    if (item.Type() == NGLayoutInlineItem::kText) {
      DCHECK_GE(item.Length(), 1u);
      if (item.Length() > 1)
        item.SetEndOffset(new_size);
      else
        items_->erase(i);
      break;
    }
    if (!item.Length()) {
      // Trailing spaces can be removed across non-character items.
      item.SetOffset(new_size, new_size);
      continue;
    }
    NOTREACHED();
    break;
  }
}

void NGLayoutInlineItemsBuilder::AppendBidiControl(const ComputedStyle* style,
                                                   UChar ltr,
                                                   UChar rtl) {
  Append(NGLayoutInlineItem::kBidiControl,
         style->Direction() == TextDirection::kRtl ? rtl : ltr);
}

void NGLayoutInlineItemsBuilder::EnterBlock(const ComputedStyle* style) {
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

void NGLayoutInlineItemsBuilder::EnterInline(LayoutObject* node) {
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
      Append(NGLayoutInlineItem::kBidiControl, kFirstStrongIsolateCharacter);
      Enter(node, kPopDirectionalIsolateCharacter);
      break;
    case UnicodeBidi::kIsolateOverride:
      Append(NGLayoutInlineItem::kBidiControl, kFirstStrongIsolateCharacter);
      AppendBidiControl(style, kLeftToRightOverrideCharacter,
                        kRightToLeftOverrideCharacter);
      Enter(node, kPopDirectionalIsolateCharacter);
      Enter(node, kPopDirectionalFormattingCharacter);
      break;
  }

  Append(NGLayoutInlineItem::kOpenTag, style, node);
}

void NGLayoutInlineItemsBuilder::Enter(LayoutObject* node,
                                       UChar character_to_exit) {
  exits_.push_back(OnExitNode{node, character_to_exit});
  has_bidi_controls_ = true;
}

void NGLayoutInlineItemsBuilder::ExitBlock() {
  Exit(nullptr);
}

void NGLayoutInlineItemsBuilder::ExitInline(LayoutObject* node) {
  DCHECK(node);

  Append(NGLayoutInlineItem::kCloseTag, node->Style(), node);

  Exit(node);
}

void NGLayoutInlineItemsBuilder::Exit(LayoutObject* node) {
  while (!exits_.IsEmpty() && exits_.back().node == node) {
    Append(NGLayoutInlineItem::kBidiControl, exits_.back().character);
    exits_.pop_back();
  }
}

}  // namespace blink
