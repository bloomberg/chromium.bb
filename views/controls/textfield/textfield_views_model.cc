// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/textfield_views_model.h"

#include <algorithm>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "gfx/font.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "views/views_delegate.h"
#include "views/controls/textfield/textfield.h"

namespace views {

TextfieldViewsModel::TextfieldViewsModel()
    : cursor_pos_(0),
      selection_begin_(0),
      is_password_(false) {
}

TextfieldViewsModel::~TextfieldViewsModel() {
}

void TextfieldViewsModel::GetFragments(TextFragments* fragments) const {
  DCHECK(fragments);
  fragments->clear();
  if (HasSelection()) {
    int begin = std::min(selection_begin_, cursor_pos_);
    int end = std::max(selection_begin_, cursor_pos_);
    if (begin != 0) {
      fragments->push_back(TextFragment(0, begin, false));
    }
    fragments->push_back(TextFragment(begin, end, true));
    int len = text_.length();
    if (end != len) {
      fragments->push_back(TextFragment(end, len, false));
    }
  } else {
    fragments->push_back(TextFragment(0, text_.length(), false));
  }
}

bool TextfieldViewsModel::SetText(const string16& text) {
  bool changed = text_ != text;
  if (changed) {
    text_ = text;
    if (cursor_pos_ > text.length()) {
      cursor_pos_ = text.length();
    }
  }
  ClearSelection();
  return changed;
}

void TextfieldViewsModel::Insert(char16 c) {
  if (HasSelection())
    DeleteSelection();
  text_.insert(cursor_pos_, 1, c);
  cursor_pos_++;
  ClearSelection();
}

void TextfieldViewsModel::Replace(char16 c) {
  if (!HasSelection())
    Delete();
  Insert(c);
}

void TextfieldViewsModel::Append(const string16& text) {
  text_ += text;
}

bool TextfieldViewsModel::Delete() {
  if (HasSelection()) {
    DeleteSelection();
    return true;
  } else if (text_.length() > cursor_pos_) {
    text_.erase(cursor_pos_, 1);
    return true;
  }
  return false;
}

bool TextfieldViewsModel::Backspace() {
  if (HasSelection()) {
    DeleteSelection();
    return true;
  } else if (cursor_pos_ > 0) {
    cursor_pos_--;
    text_.erase(cursor_pos_, 1);
    ClearSelection();
    return true;
  }
  return false;
}

void TextfieldViewsModel::MoveCursorLeft(bool select) {
  // TODO(oshima): support BIDI
  if (select) {
    if (cursor_pos_ > 0)
      cursor_pos_--;
  } else {
    if (HasSelection())
      cursor_pos_ = std::min(cursor_pos_, selection_begin_);
    else if (cursor_pos_ > 0)
      cursor_pos_--;
    ClearSelection();
  }
}

void TextfieldViewsModel::MoveCursorRight(bool select) {
  // TODO(oshima): support BIDI
  if (select) {
    cursor_pos_ = std::min(text_.length(),  cursor_pos_ + 1);
  } else {
    if  (HasSelection())
      cursor_pos_ = std::max(cursor_pos_, selection_begin_);
    else
      cursor_pos_ = std::min(text_.length(),  cursor_pos_ + 1);
    ClearSelection();
  }
}

void TextfieldViewsModel::MoveCursorToPreviousWord(bool select) {
  // Notes: We always iterate words from the begining.
  // This is probably fast enough for our usage, but we may
  // want to modify WordIterator so that it can start from the
  // middle of string and advance backwards.
  base::BreakIterator iter(&text_, base::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return;
  int last = 0;
  while (iter.Advance()) {
    if (iter.IsWord()) {
      size_t begin = iter.pos() - iter.GetString().length();
      if (begin == cursor_pos_) {
        // The cursor is at the beginning of a word.
        // Move to previous word.
        break;
      } else if(iter.pos() >= cursor_pos_) {
        // The cursor is in the middle or at the end of a word.
        // Move to the top of current word.
        last = begin;
        break;
      } else {
        last = iter.pos() - iter.GetString().length();
      }
    }
  }

  cursor_pos_ = last;
  if (!select)
    ClearSelection();
}

void TextfieldViewsModel::MoveCursorToNextWord(bool select) {
  base::BreakIterator iter(&text_, base::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return;
  size_t pos = 0;
  while (iter.Advance()) {
    pos = iter.pos();
    if (iter.IsWord() && pos > cursor_pos_) {
      break;
    }
  }
  cursor_pos_ = pos;
  if (!select)
    ClearSelection();
}

void TextfieldViewsModel::MoveCursorToStart(bool select) {
  cursor_pos_ = 0;
  if (!select)
    ClearSelection();
}

void TextfieldViewsModel::MoveCursorToEnd(bool select) {
  cursor_pos_ = text_.length();
  if (!select)
    ClearSelection();
}

bool TextfieldViewsModel::MoveCursorTo(size_t pos, bool select) {
  bool cursor_changed = false;
  if (cursor_pos_ != pos) {
    cursor_pos_ = pos;
    cursor_changed = true;
  }
  if (!select)
    ClearSelection();
  return cursor_changed;
}

gfx::Rect TextfieldViewsModel::GetCursorBounds(const gfx::Font& font) const {
  string16 text = GetVisibleText();
  int x = font.GetStringWidth(text.substr(0U, cursor_pos_));
  int h = font.GetHeight();
  DCHECK(x >= 0);
  if (text.length() == cursor_pos_) {
    return gfx::Rect(x, 0, 0, h);
  } else {
    int x_end = font.GetStringWidth(text.substr(0U, cursor_pos_ + 1U));
    return gfx::Rect(x, 0, x_end - x, h);
  }
}

string16 TextfieldViewsModel::GetSelectedText() const {
  return text_.substr(
      std::min(cursor_pos_, selection_begin_),
      std::abs(static_cast<long>(cursor_pos_ - selection_begin_)));
}

void TextfieldViewsModel::GetSelectedRange(TextRange* range) const {
  range->SetRange(selection_begin_, cursor_pos_);
}

void TextfieldViewsModel::SelectRange(const TextRange& range) {
  selection_begin_ = GetSafePosition(range.start());
  cursor_pos_ = GetSafePosition(range.end());
}

void TextfieldViewsModel::SelectAll() {
  // SelectAll selects towards the end.
  cursor_pos_ = text_.length();
  selection_begin_ = 0;
}

void TextfieldViewsModel::SelectWord() {
  // First we setup selection_begin_ and cursor_pos_. There are so many cases
  // because we try to emulate what select-word looks like in a gtk textfield.
  // See associated testcase for different cases.
  if (cursor_pos_ > 0 && cursor_pos_ < text_.length()) {
    if (isalnum(text_[cursor_pos_])) {
      selection_begin_ = cursor_pos_;
      cursor_pos_++;
    } else
      selection_begin_ = cursor_pos_ - 1;
  } else if (cursor_pos_ == 0) {
    selection_begin_ = cursor_pos_;
    if (text_.length() > 0)
      cursor_pos_++;
  } else {
    selection_begin_ = cursor_pos_ - 1;
  }

  // Now we move selection_begin_ to beginning of selection. Selection boundary
  // is defined as the position where we have alpha-num character on one side
  // and non-alpha-num char on the other side.
  for (; selection_begin_ > 0; selection_begin_--) {
    if (IsPositionAtWordSelectionBoundary(selection_begin_))
      break;
  }

  // Now we move cursor_pos_ to end of selection. Selection boundary
  // is defined as the position where we have alpha-num character on one side
  // and non-alpha-num char on the other side.
  for (; cursor_pos_ < text_.length(); cursor_pos_++) {
    if (IsPositionAtWordSelectionBoundary(cursor_pos_))
      break;
  }
}

void TextfieldViewsModel::ClearSelection() {
  selection_begin_ = cursor_pos_;
}

bool TextfieldViewsModel::Cut() {
  if (HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
    DeleteSelection();
    return true;
  }
  return false;
}

void TextfieldViewsModel::Copy() {
  if (HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
  }
}

bool TextfieldViewsModel::Paste() {
  string16 result;
  views::ViewsDelegate::views_delegate->GetClipboard()
      ->ReadText(ui::Clipboard::BUFFER_STANDARD, &result);
  if (!result.empty()) {
    if (HasSelection())
      DeleteSelection();
    text_.insert(cursor_pos_, result);
    cursor_pos_ += result.length();
    ClearSelection();
    return true;
  }
  return false;
}

bool TextfieldViewsModel::HasSelection() const {
  return selection_begin_ != cursor_pos_;
}

void TextfieldViewsModel::DeleteSelection() {
  DCHECK(HasSelection());
  size_t n = std::abs(static_cast<long>(cursor_pos_ - selection_begin_));
  size_t begin = std::min(cursor_pos_, selection_begin_);
  text_.erase(begin, n);
  cursor_pos_ = begin;
  ClearSelection();
}

string16 TextfieldViewsModel::GetVisibleText(size_t begin, size_t end) const {
  DCHECK(end >= begin);
  if (is_password_) {
    return string16(end - begin, '*');
  }
  return text_.substr(begin, end - begin);
}

bool TextfieldViewsModel::IsPositionAtWordSelectionBoundary(size_t pos) {
  return (isalnum(text_[pos - 1]) && !isalnum(text_[pos])) ||
      (!isalnum(text_[pos - 1]) && isalnum(text_[pos]));
}

size_t TextfieldViewsModel::GetSafePosition(size_t position) const {
  if (position > text_.length()) {
    return text_.length();
  }
  return position;
}

}  // namespace views
