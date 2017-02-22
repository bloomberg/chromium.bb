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
  DCHECK_EQ(text_.length(), items_->isEmpty() ? 0 : items_->back().EndOffset());
}

String NGLayoutInlineItemsBuilder::ToString() {
  // Segment Break Transformation Rules[1] defines to keep trailing new lines,
  // but it will be removed in Phase II[2]. We prefer not to add trailing new
  // lines and collapsible spaces in Phase I.
  // [1] https://drafts.csswg.org/css-text-3/#line-break-transform
  // [2] https://drafts.csswg.org/css-text-3/#white-space-phase-2
  unsigned next_start_offset = text_.length();
  RemoveTrailingCollapsibleSpace(&next_start_offset);

  return text_.toString();
}

// Determine "Ambiguous" East Asian Width is Wide or Narrow.
// Unicode East Asian Width
// http://unicode.org/reports/tr11/
static bool IsAmbiguosEastAsianWidthWide(const ComputedStyle* style) {
  UScriptCode script = style->getFontDescription().script();
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
  if (!before.isEmpty()) {
    last = before[before.length() - 1];
    if (last == zeroWidthSpaceCharacter)
      return true;
  }
  UChar32 next = 0;
  if (after_index < after.length()) {
    next = after[after_index];
    if (next == zeroWidthSpaceCharacter)
      return true;
  }

  // Logic below this point requires both before and after be 16 bits.
  if (before.is8Bit() || after.is8Bit())
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
  return (!before.is8Bit() || !after.is8Bit()) &&
         ShouldRemoveNewlineSlow(before, before_style, after, after_index,
                                 after_style);
}

static void AppendItem(Vector<NGLayoutInlineItem>* items,
                       unsigned start,
                       unsigned end,
                       const ComputedStyle* style = nullptr,
                       LayoutObject* layout_object = nullptr) {
  DCHECK(items->isEmpty() || items->back().EndOffset() == start);
  DCHECK_LT(start, end);
  items->push_back(NGLayoutInlineItem(start, end, style, layout_object));
}

void NGLayoutInlineItemsBuilder::Append(const String& string,
                                        const ComputedStyle* style,
                                        LayoutObject* layout_object) {
  if (string.isEmpty())
    return;

  if (has_pending_newline_)
    ProcessPendingNewline(string, style);

  EWhiteSpace whitespace = style->whiteSpace();
  bool preserve_newline =
      ComputedStyle::preserveNewline(whitespace) && !is_svgtext_;
  bool collapse_whitespace = ComputedStyle::collapseWhiteSpace(whitespace);
  unsigned start_offset = text_.length();

  if (!collapse_whitespace) {
    text_.append(string);
    is_last_collapsible_space_ = false;
  } else {
    text_.reserveCapacity(string.length());
    for (unsigned i = 0; i < string.length(); i++) {
      UChar c = string[i];
      bool is_collapsible_space;
      if (c == newlineCharacter) {
        RemoveTrailingCollapsibleSpace(&start_offset);
        if (preserve_newline) {
          text_.append(c);
          // Remove collapsible spaces immediately following a newline.
          is_last_collapsible_space_ = true;
          continue;
        }
        if (i + 1 == string.length()) {
          // If at the end of string, process this newline on the next Append.
          has_pending_newline_ = true;
          break;
        }
        if (ShouldRemoveNewline(text_, style, string, i + 1, style))
          continue;
        is_collapsible_space = true;
      } else {
        is_collapsible_space = c == spaceCharacter || c == tabulationCharacter;
      }
      if (!is_collapsible_space) {
        text_.append(c);
        is_last_collapsible_space_ = false;
      } else if (!is_last_collapsible_space_) {
        text_.append(spaceCharacter);
        is_last_collapsible_space_ = true;
      }
    }
  }

  if (text_.length() > start_offset)
    AppendItem(items_, start_offset, text_.length(), style, layout_object);
}

void NGLayoutInlineItemsBuilder::Append(UChar character,
                                        const ComputedStyle* style,
                                        LayoutObject* layout_object) {
  DCHECK(character != spaceCharacter && character != tabulationCharacter &&
         character != newlineCharacter && character != zeroWidthSpaceCharacter);
  if (has_pending_newline_)
    ProcessPendingNewline(emptyString, nullptr);

  text_.append(character);
  unsigned end_offset = text_.length();
  AppendItem(items_, end_offset - 1, end_offset, style, layout_object);
  is_last_collapsible_space_ = false;
}

void NGLayoutInlineItemsBuilder::AppendAsOpaqueToSpaceCollapsing(
    UChar character) {
  if (has_pending_newline_)
    ProcessPendingNewline(emptyString, nullptr);

  text_.append(character);
  unsigned end_offset = text_.length();
  AppendItem(items_, end_offset - 1, end_offset, nullptr);
}

void NGLayoutInlineItemsBuilder::ProcessPendingNewline(
    const String& string,
    const ComputedStyle* style) {
  DCHECK(has_pending_newline_);
  if (!items_->isEmpty()) {
    NGLayoutInlineItem& item = items_->back();
    if (!ShouldRemoveNewline(text_, item.Style(), string, 0, style)) {
      text_.append(spaceCharacter);
      item.SetEndOffset(text_.length());
    }
  }
  // Remove spaces following a newline even when the newline was removed.
  is_last_collapsible_space_ = true;
  has_pending_newline_ = false;
}

void NGLayoutInlineItemsBuilder::RemoveTrailingCollapsibleSpace(
    unsigned* next_start_offset) {
  if (!is_last_collapsible_space_ || text_.isEmpty())
    return;
  DCHECK_EQ(spaceCharacter, text_[text_.length() - 1]);
  unsigned new_size = text_.length() - 1;
  text_.resize(new_size);
  is_last_collapsible_space_ = false;

  // Adjust the last item if the removed space is already appended.
  if (*next_start_offset > new_size) {
    *next_start_offset = new_size;
    if (!items_->isEmpty()) {
      NGLayoutInlineItem& last_item = items_->back();
      DCHECK_EQ(last_item.EndOffset(), new_size + 1);
      if (last_item.StartOffset() == new_size)
        items_->pop_back();
      else
        last_item.SetEndOffset(new_size);
    }
  }
}

void NGLayoutInlineItemsBuilder::AppendBidiControl(const ComputedStyle* style,
                                                   UChar ltr,
                                                   UChar rtl) {
  AppendAsOpaqueToSpaceCollapsing(
      style->direction() == TextDirection::kRtl ? rtl : ltr);
}

void NGLayoutInlineItemsBuilder::EnterBlock(const ComputedStyle* style) {
  // Handle bidi-override on the block itself.
  // Isolate and embed values are enforced by default and redundant on the block
  // elements.
  // Plaintext and direction are handled as the paragraph level by
  // NGBidiParagraph::SetParagraph().
  if (style->getUnicodeBidi() == UnicodeBidi::kBidiOverride ||
      style->getUnicodeBidi() == UnicodeBidi::kIsolateOverride) {
    AppendBidiControl(style, leftToRightOverrideCharacter,
                      rightToLeftOverrideCharacter);
    Enter(nullptr, popDirectionalFormattingCharacter);
  }
}

void NGLayoutInlineItemsBuilder::EnterInline(LayoutObject* node) {
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  const ComputedStyle* style = node->style();
  switch (style->getUnicodeBidi()) {
    case UnicodeBidi::kNormal:
      break;
    case UnicodeBidi::kEmbed:
      AppendBidiControl(style, leftToRightEmbedCharacter,
                        rightToLeftEmbedCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
    case UnicodeBidi::kBidiOverride:
      AppendBidiControl(style, leftToRightOverrideCharacter,
                        rightToLeftOverrideCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
    case UnicodeBidi::kIsolate:
      AppendBidiControl(style, leftToRightIsolateCharacter,
                        rightToLeftIsolateCharacter);
      Enter(node, popDirectionalIsolateCharacter);
      break;
    case UnicodeBidi::kPlaintext:
      AppendAsOpaqueToSpaceCollapsing(firstStrongIsolateCharacter);
      Enter(node, popDirectionalIsolateCharacter);
      break;
    case UnicodeBidi::kIsolateOverride:
      AppendAsOpaqueToSpaceCollapsing(firstStrongIsolateCharacter);
      AppendBidiControl(style, leftToRightOverrideCharacter,
                        rightToLeftOverrideCharacter);
      Enter(node, popDirectionalIsolateCharacter);
      Enter(node, popDirectionalFormattingCharacter);
      break;
  }
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
  Exit(node);
}

void NGLayoutInlineItemsBuilder::Exit(LayoutObject* node) {
  while (!exits_.isEmpty() && exits_.back().node == node) {
    AppendAsOpaqueToSpaceCollapsing(exits_.back().character);
    exits_.pop_back();
  }
}

}  // namespace blink
