// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_text_utils.h"

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// TODO(accessibility): Extend this or switch to using ICU in order to handle
// languages other than English.
bool IsSentenceEndingPunctuation(base::char16 character) {
  return character == '.' || character == '!' || character == '?';
}

bool IsInterSentenceWordCharacter(base::char16 character) {
  return !base::IsUnicodeWhitespace(character) &&
         !IsSentenceEndingPunctuation(character);
}

bool AlreadyPastSentenceEndingPunctuation(const base::string16& text,
                                          size_t start_offset) {
  auto i = text.rbegin() + (text.size() - start_offset);
  auto found = std::find_if_not(i, text.rend(), base::IsUnicodeWhitespace);
  return found != text.rend() && IsSentenceEndingPunctuation(*found);
}

}  // namespace

// line_breaks is a Misnomer. Blink provides the start offsets of each line
// not the line breaks.
// TODO(nektar): Rename line_breaks a11y attribute and variable references.
size_t FindAccessibleTextBoundary(const base::string16& text,
                                  const std::vector<int>& line_breaks,
                                  TextBoundaryType boundary,
                                  size_t start_offset,
                                  TextBoundaryDirection direction,
                                  ax::mojom::TextAffinity affinity) {
  size_t text_size = text.size();
  DCHECK_LE(start_offset, text_size);

  if (boundary == CHAR_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION && start_offset < text_size)
      return start_offset + 1;
    else
      return start_offset;
  }

  base::i18n::BreakIterator word_iter(text,
                                      base::i18n::BreakIterator::BREAK_WORD);
  if (boundary == WORD_BOUNDARY) {
    if (!word_iter.Init())
      return start_offset;
  }

  if (boundary == LINE_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION) {
      for (size_t j = 0; j < line_breaks.size(); ++j) {
          size_t line_break = line_breaks[j] >= 0 ? line_breaks[j] : 0;
          if ((affinity == ax::mojom::TextAffinity::kDownstream &&
               line_break > start_offset) ||
              (affinity == ax::mojom::TextAffinity::kUpstream &&
               line_break >= start_offset)) {
            return line_break;
        }
      }
      return text_size;
    } else {
      for (size_t j = line_breaks.size(); j != 0; --j) {
        size_t line_break = line_breaks[j - 1] >= 0 ? line_breaks[j - 1] : 0;
        if ((affinity == ax::mojom::TextAffinity::kDownstream &&
             line_break <= start_offset) ||
            (affinity == ax::mojom::TextAffinity::kUpstream &&
             line_break < start_offset)) {
          return line_break;
        }
      }
      return 0;
    }
  }

  // Given the string "One sentence. Two sentences.   Three sentences.", the
  // boundaries of the middle sentence should give a string like "Two
  // sentences.   " This means there are two different starting situations when
  // searching forwards for the sentence boundary:
  //
  // The first situation is when the starting index is somewhere in the
  // whitespace between the last sentence-ending punctuation of a sentence and
  // before the start of the next sentence. Since this part of the string is
  // considered part of the previous sentence, we need to scan forward for the
  // first non-whitespace and non-punctuation character.
  //
  // The second situation is when the starting index is somewhere on or before
  // the first sentence-ending punctuation that ends the sentence, but still
  // after the first non-whitespace character. In this case, we need to find
  // the first sentence-ending punctuation and then to follow the procedure for
  // the first situation.
  //
  // In order to know what situation we are in, we first need to scan backward
  // to see if we are already past the first sentence-ending punctuation.
  bool already_past_sentence_ending_punctuation = false;
  if (boundary == SENTENCE_BOUNDARY && direction == FORWARDS_DIRECTION)
    already_past_sentence_ending_punctuation =
        AlreadyPastSentenceEndingPunctuation(text, start_offset);

  // When searching backward for the start of a sentence we need to look for
  // the punctuation that ended the previous sentence and then return the first
  // non-whitespace character that follows.
  base::Optional<size_t> offset_of_last_seen_word_character = base::nullopt;

  size_t result = start_offset;
  for (;;) {
    size_t pos;
    if (direction == FORWARDS_DIRECTION) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result == 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case CHAR_BOUNDARY:
      case LINE_BOUNDARY:
        NOTREACHED();  // These are handled above.
        break;
      case WORD_BOUNDARY:
        if (word_iter.IsStartOfWord(result)) {
          // If we are searching forward and we are still at the start offset,
          // we need to find the next word.
          if (direction == BACKWARDS_DIRECTION || result != start_offset)
            return result;
        }
        break;
      case PARAGRAPH_BOUNDARY:
        if (text[pos] == '\n')
          return result;
        break;
      case SENTENCE_BOUNDARY:
        if (direction == FORWARDS_DIRECTION) {
          if (already_past_sentence_ending_punctuation &&
              IsInterSentenceWordCharacter(text[pos]))
            return result;
          if (IsSentenceEndingPunctuation(text[pos]))
            already_past_sentence_ending_punctuation = true;
        } else if (direction == BACKWARDS_DIRECTION) {
          if (IsInterSentenceWordCharacter(text[pos]))
            offset_of_last_seen_word_character = pos;
          if (offset_of_last_seen_word_character.has_value() &&
              IsSentenceEndingPunctuation(text[pos]))
            return *offset_of_last_seen_word_character;
        }
        break;
      case ALL_BOUNDARY:
      default:
        break;
    }

    if (direction == FORWARDS_DIRECTION) {
      result++;
    } else {
      result--;
    }
  }
}

base::string16 ActionVerbToLocalizedString(
    const ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return base::string16();
    case ax::mojom::DefaultActionVerb::kActivate:
      return l10n_util::GetStringUTF16(IDS_AX_ACTIVATE_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kCheck:
      return l10n_util::GetStringUTF16(IDS_AX_CHECK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClick:
      return l10n_util::GetStringUTF16(IDS_AX_CLICK_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return l10n_util::GetStringUTF16(IDS_AX_CLICK_ANCESTOR_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kJump:
      return l10n_util::GetStringUTF16(IDS_AX_JUMP_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kOpen:
      return l10n_util::GetStringUTF16(IDS_AX_OPEN_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kPress:
      return l10n_util::GetStringUTF16(IDS_AX_PRESS_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kSelect:
      return l10n_util::GetStringUTF16(IDS_AX_SELECT_ACTION_VERB);
    case ax::mojom::DefaultActionVerb::kUncheck:
      return l10n_util::GetStringUTF16(IDS_AX_UNCHECK_ACTION_VERB);
  }
  NOTREACHED();
  return base::string16();
}

// Some APIs on Linux and Windows need to return non-localized action names.
base::string16 ActionVerbToUnlocalizedString(
    const ax::mojom::DefaultActionVerb action_verb) {
  switch (action_verb) {
    case ax::mojom::DefaultActionVerb::kNone:
      return base::UTF8ToUTF16("none");
    case ax::mojom::DefaultActionVerb::kActivate:
      return base::UTF8ToUTF16("activate");
    case ax::mojom::DefaultActionVerb::kCheck:
      return base::UTF8ToUTF16("check");
    case ax::mojom::DefaultActionVerb::kClick:
      return base::UTF8ToUTF16("click");
    case ax::mojom::DefaultActionVerb::kClickAncestor:
      return base::UTF8ToUTF16("click-ancestor");
    case ax::mojom::DefaultActionVerb::kJump:
      return base::UTF8ToUTF16("jump");
    case ax::mojom::DefaultActionVerb::kOpen:
      return base::UTF8ToUTF16("open");
    case ax::mojom::DefaultActionVerb::kPress:
      return base::UTF8ToUTF16("press");
    case ax::mojom::DefaultActionVerb::kSelect:
      return base::UTF8ToUTF16("select");
    case ax::mojom::DefaultActionVerb::kUncheck:
      return base::UTF8ToUTF16("uncheck");
  }
  NOTREACHED();
  return base::string16();
}

}  // namespace ui
