// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/textfield_views_model.h"

#include <algorithm>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/range/range.h"
#include "ui/gfx/font.h"
#include "views/controls/textfield/textfield.h"
#include "views/views_delegate.h"

namespace views {

namespace internal {

// An edit object holds enough information/state to undo/redo the
// change. Two edits are merged when possible, for example, when
// you type new characters in sequence.  |Commit()| can be used to
// mark an edit as an independent edit and it shouldn't be merged.
// (For example, when you did undo/redo, or a text is appended via
// API)
// TODO(oshima): Try to consolidate edit classes as one Edit. At least,
// there should be one method for each undo and redo.
class Edit {
 public:
  virtual ~Edit() {}

  enum Type {
    INSERT_EDIT,
    DELETE_EDIT,
    REPLACE_EDIT
  };

  // Apply undo operation to the given |model|.
  virtual void Undo(TextfieldViewsModel* model) = 0;

  // Apply redo operation to the given |model|.
  virtual void Redo(TextfieldViewsModel* model) = 0;

  // Tries to merge the given edit to itself. Returns false
  // if merge fails.
  virtual bool Merge(Edit* edit) = 0;

  Type type() { return type_; }

  // Can this edit be merged?
  bool mergeable() { return mergeable_; }

  // Commits the edit and marks as un-mergeable.
  void Commit() { mergeable_ = false; }

 protected:
  Edit(Type type, bool mergeable)
      : mergeable_(mergeable),
        type_(type) {}

  // True if the edit can be marged.
  bool mergeable_;

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(Edit);
};

}  // namespace internal

namespace {

// An edit to insert text. Insertion edits can be merged only if
// next insertion starts with the last insertion point (continuous).
class InsertEdit : public internal::Edit {
 public:
  InsertEdit(bool mergeable, size_t cursor_pos, const string16& new_text)
      : Edit(INSERT_EDIT, mergeable),
        cursor_pos_(cursor_pos),
        new_text_(new_text) {
  }
  virtual ~InsertEdit() {}

  // internal::Edit implementation:
  virtual void Undo(TextfieldViewsModel* model) OVERRIDE {
    if (new_text_.empty())
        return;
    model->SelectRange(
        ui::Range(cursor_pos_, cursor_pos_ + new_text_.length()));
    model->DeleteSelection(false);
  }

  virtual void Redo(TextfieldViewsModel* model) OVERRIDE {
    model->MoveCursorTo(cursor_pos_, false);
    if (new_text_.empty())
        return;
    model->InsertText(new_text_);
  }

  virtual bool Merge(Edit* edit) OVERRIDE {
    if (edit->type() != INSERT_EDIT || !mergeable_ || !edit->mergeable())
      return false;

    InsertEdit* insert_edit = static_cast<InsertEdit*>(edit);
    // If continuous edit, merge it.
    if (cursor_pos_ + new_text_.length() != insert_edit->cursor_pos_)
      return false;
    // TODO(oshima): gtk splits edits between whitespace. Find out what
    // we want to here and implement if necessary.
    new_text_ += insert_edit->new_text_;
    return true;
  }

 private:
  friend class ReplaceEdit;
  // A cursor position at which the new_text_ is added.
  size_t cursor_pos_;
  string16 new_text_;

  DISALLOW_COPY_AND_ASSIGN(InsertEdit);
};

// An edit to replace text. ReplaceEdit can be merged with either InsertEdit or
// ReplaceEdit when they're continuous.
class ReplaceEdit : public internal::Edit {
 public:
  ReplaceEdit(bool mergeable,
              size_t cursor_pos,
              int deleted_size,
              const string16& old_text,
              const string16& new_text)
      : Edit(REPLACE_EDIT, mergeable),
        cursor_pos_(cursor_pos),
        deleted_size_(deleted_size),
        old_text_(old_text),
        new_text_(new_text) {
  }

  virtual ~ReplaceEdit() {
  }

  // internal::Edit implementation:
  virtual void Undo(TextfieldViewsModel* model) OVERRIDE {
    size_t cursor_pos = std::min(cursor_pos_,
                                 cursor_pos_ + deleted_size_);
    model->SelectRange(
        ui::Range(cursor_pos, cursor_pos + new_text_.length()));
    model->DeleteSelection(false);
    model->InsertText(old_text_);
    model->MoveCursorTo(cursor_pos_, false);
  }

  virtual void Redo(TextfieldViewsModel* model) OVERRIDE {
    size_t cursor_pos = std::min(cursor_pos_,
                                 cursor_pos_ + deleted_size_);
    model->SelectRange(
        ui::Range(cursor_pos, cursor_pos + old_text_.length()));
    model->DeleteSelection(false);
    model->InsertText(new_text_);
  }

  virtual bool Merge(Edit* edit) OVERRIDE {
    if (edit->type() == DELETE_EDIT || !mergeable_ || !edit->mergeable())
      return false;
    if (edit->type() == INSERT_EDIT) {
      return MergeInsert(static_cast<InsertEdit*>(edit));
    } else {
      DCHECK_EQ(REPLACE_EDIT, edit->type());
      return MergeReplace(static_cast<ReplaceEdit*>(edit));
    }
  }

 private:
  bool MergeInsert(InsertEdit* insert_edit) {
    if (deleted_size_ < 0) {
      if (cursor_pos_ + deleted_size_ + new_text_.length()
          != insert_edit->cursor_pos_)
        return false;
    } else {
      if (cursor_pos_ + new_text_.length() != insert_edit->cursor_pos_)
        return false;
    }
    new_text_ += insert_edit->new_text_;
    return true;
  }

  bool MergeReplace(ReplaceEdit* replace_edit) {
    if (deleted_size_ > 0) {
      if (cursor_pos_ + new_text_.length() != replace_edit->cursor_pos_)
        return false;
    } else {
      if (cursor_pos_ + deleted_size_ + new_text_.length()
          != replace_edit->cursor_pos_)
        return false;
    }
    old_text_ += replace_edit->old_text_;
    new_text_ += replace_edit->new_text_;
    return true;
  }
  // A cursor position of the selected text that was replaced.
  size_t cursor_pos_;
  // Index difference of the selected text that was replaced.
  // This is negative if the selection is made backward.
  int deleted_size_;
  string16 old_text_;
  string16 new_text_;

  DISALLOW_COPY_AND_ASSIGN(ReplaceEdit);
};

// An edit for deletion. Delete edits can be merged only if two
// deletions have the same direction, and are continuous.
class DeleteEdit : public internal::Edit {
 public:
  DeleteEdit(bool mergeable,
             size_t cursor_pos, int size,
             const string16& text)
      : Edit(DELETE_EDIT, mergeable),
        cursor_pos_(cursor_pos),
        size_(size),
        old_text_(text) {
  }
  virtual ~DeleteEdit() {
  }

  // internal::Edit implementation:
  virtual void Undo(TextfieldViewsModel* model) OVERRIDE {
    size_t cursor_pos = std::min(cursor_pos_,
                                 cursor_pos_ + size_);
    model->MoveCursorTo(cursor_pos, false);
    model->InsertText(old_text_);
    model->MoveCursorTo(cursor_pos_, false);
  }

  virtual void Redo(TextfieldViewsModel* model) OVERRIDE {
    model->SelectRange(
        ui::Range(cursor_pos_, cursor_pos_ + size_));
    model->DeleteSelection(false);
  }

  virtual bool Merge(Edit* edit) OVERRIDE {
    if (edit->type() != DELETE_EDIT || !mergeable_ || !edit->mergeable())
      return false;

    DeleteEdit* delete_edit = static_cast<DeleteEdit*>(edit);
    if (size_ < 0) {
      // backspace can be merged only with backspace at the
      // same position.
      if (delete_edit->size_ > 0 ||
          cursor_pos_ + size_ != delete_edit->cursor_pos_)
        return false;
      old_text_ = delete_edit->old_text_ + old_text_;
    } else {
      // delete can be merged only with delete at the same
      // position.
      if (delete_edit->size_ < 0 ||
          cursor_pos_ != delete_edit->cursor_pos_)
        return false;
      old_text_ += delete_edit->old_text_;
    }
    size_ += delete_edit->size_;
    return true;
  }

 private:
  // A cursor position where the deletion started.
  size_t cursor_pos_;
  // Number of characters deleted. This is positive for delete and
  // negative for backspace operation.
  int size_;
  // Deleted text.
  string16 old_text_;

  DISALLOW_COPY_AND_ASSIGN(DeleteEdit);
};

}  // namespace

/////////////////////////////////////////////////////////////////
// TextfieldViewsModel: public

TextfieldViewsModel::Delegate::~Delegate() {
}

TextfieldViewsModel::TextfieldViewsModel(Delegate* delegate)
    : delegate_(delegate),
      cursor_pos_(0),
      selection_start_(0),
      composition_start_(0),
      composition_end_(0),
      is_password_(false),
      current_edit_(edit_history_.end()),
      update_history_(true) {
}

TextfieldViewsModel::~TextfieldViewsModel() {
  ClearEditHistory();
}

void TextfieldViewsModel::GetFragments(TextFragments* fragments) const {
  DCHECK(fragments);
  fragments->clear();
  if (HasCompositionText()) {
    if (composition_start_)
      fragments->push_back(TextFragment(0, composition_start_, false, false));

    size_t selection_start = std::min(selection_start_, cursor_pos_);
    size_t selection_end = std::max(selection_start_, cursor_pos_);
    size_t last_end = composition_start_;
    for (ui::CompositionUnderlines::const_iterator i =
         composition_underlines_.begin();
         i != composition_underlines_.end(); ++i) {
      size_t fragment_start =
          std::min(i->start_offset, i->end_offset) + composition_start_;
      size_t fragment_end =
          std::max(i->start_offset, i->end_offset) + composition_start_;

      fragment_start = std::max(last_end, fragment_start);
      fragment_end = std::min(fragment_end, composition_end_);

      if (fragment_start >= fragment_end)
        break;

      // If there is no selection, then just add a text fragment with underline.
      if (selection_start == selection_end) {
        if (last_end < fragment_start) {
          fragments->push_back(
              TextFragment(last_end, fragment_start, false, false));
        }
        fragments->push_back(
            TextFragment(fragment_start, fragment_end, false, true));
        last_end = fragment_end;
        continue;
      }

      size_t end = std::min(fragment_start, selection_start);
      if (last_end < end)
        fragments->push_back(TextFragment(last_end, end, false, false));

      last_end = fragment_end;

      if (selection_start < fragment_start) {
        end = std::min(selection_end, fragment_start);
        fragments->push_back(TextFragment(selection_start, end, true, false));
        selection_start = end;
      } else if (selection_start > fragment_start) {
        end = std::min(selection_start, fragment_end);
        fragments->push_back(TextFragment(fragment_start, end, false, true));
        fragment_start = end;
        if (fragment_start == fragment_end)
          continue;
      }

      if (fragment_start < selection_end) {
        DCHECK_EQ(selection_start, fragment_start);
        end = std::min(fragment_end, selection_end);
        fragments->push_back(TextFragment(fragment_start, end, true, true));
        fragment_start = end;
        selection_start = end;
        if (fragment_start == fragment_end)
          continue;
      }

      DCHECK_LT(fragment_start, fragment_end);
      fragments->push_back(
          TextFragment(fragment_start, fragment_end, false, true));
    }

    if (last_end < composition_end_) {
      if (selection_start < selection_end) {
        DCHECK_LE(last_end, selection_start);
        if (last_end < selection_start) {
          fragments->push_back(
              TextFragment(last_end, selection_start, false, false));
        }
        fragments->push_back(
            TextFragment(selection_start, selection_end, true, false));
        if (selection_end < composition_end_) {
          fragments->push_back(
              TextFragment(selection_end, composition_end_, false, false));
        }
      } else {
        fragments->push_back(
            TextFragment(last_end, composition_end_, false, false));
      }
    }

    size_t len = text_.length();
    if (composition_end_ < len)
      fragments->push_back(TextFragment(composition_end_, len, false, false));
  } else if (HasSelection()) {
    size_t start = std::min(selection_start_, cursor_pos_);
    size_t end = std::max(selection_start_, cursor_pos_);
    if (start)
      fragments->push_back(TextFragment(0, start, false, false));
    fragments->push_back(TextFragment(start, end, true, false));
    size_t len = text_.length();
    if (end != len)
      fragments->push_back(TextFragment(end, len, false, false));
  } else {
    fragments->push_back(TextFragment(0, text_.length(), false, false));
  }
}

bool TextfieldViewsModel::SetText(const string16& text) {
  bool changed = false;
  if (HasCompositionText()) {
    ConfirmCompositionText();
    changed = true;
  }
  if (text_ != text) {
    if (changed) // no need to remember composition.
      Undo();
    SelectAll();
    InsertTextInternal(text, false);
    cursor_pos_ = 0;
  }
  ClearSelection();
  return changed;
}

void TextfieldViewsModel::Append(const string16& text) {
  if (HasCompositionText())
    ConfirmCompositionText();
  size_t save = cursor_pos_;
  MoveCursorToEnd(false);
  InsertText(text);
  cursor_pos_ = save;
  ClearSelection();
}

bool TextfieldViewsModel::Delete() {
  if (HasCompositionText()) {
    // No undo/redo for composition text.
    ClearCompositionText();
    return true;
  }
  if (HasSelection()) {
    DeleteSelection(true);
    return true;
  }
  if (text_.length() > cursor_pos_) {
    RecordDelete(cursor_pos_, 1, true);
    text_.erase(cursor_pos_, 1);
    return true;
  }
  return false;
}

bool TextfieldViewsModel::Backspace() {
  if (HasCompositionText()) {
    // No undo/redo for composition text.
    ClearCompositionText();
    return true;
  }
  if (HasSelection()) {
    DeleteSelection(true);
    return true;
  }
  if (cursor_pos_ > 0) {
    RecordDelete(cursor_pos_, -1, true);
    cursor_pos_--;
    text_.erase(cursor_pos_, 1);
    ClearSelection();
    return true;
  }
  return false;
}

void TextfieldViewsModel::MoveCursorLeft(bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  // TODO(oshima): support BIDI
  if (select) {
    if (cursor_pos_ > 0)
      cursor_pos_--;
  } else {
    if (HasSelection())
      cursor_pos_ = std::min(cursor_pos_, selection_start_);
    else if (cursor_pos_ > 0)
      cursor_pos_--;
    ClearSelection();
  }
}

void TextfieldViewsModel::MoveCursorRight(bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  // TODO(oshima): support BIDI
  if (select) {
    cursor_pos_ = std::min(text_.length(),  cursor_pos_ + 1);
  } else {
    if  (HasSelection())
      cursor_pos_ = std::max(cursor_pos_, selection_start_);
    else
      cursor_pos_ = std::min(text_.length(),  cursor_pos_ + 1);
    ClearSelection();
  }
}

void TextfieldViewsModel::MoveCursorToPreviousWord(bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
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
  if (HasCompositionText())
    ConfirmCompositionText();
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

void TextfieldViewsModel::MoveCursorToHome(bool select) {
  MoveCursorTo(0, select);
}

void TextfieldViewsModel::MoveCursorToEnd(bool select) {
  MoveCursorTo(text_.length(), select);
}

bool TextfieldViewsModel::MoveCursorTo(size_t pos, bool select) {
  if (HasCompositionText())
    ConfirmCompositionText();
  bool changed = cursor_pos_ != pos || select != HasSelection();
  cursor_pos_ = pos;
  if (!select)
    ClearSelection();
  return changed;
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
      std::min(cursor_pos_, selection_start_),
      std::abs(static_cast<long>(cursor_pos_ - selection_start_)));
}

void TextfieldViewsModel::GetSelectedRange(ui::Range* range) const {
  *range = ui::Range(selection_start_, cursor_pos_);
}

void TextfieldViewsModel::SelectRange(const ui::Range& range) {
  if (HasCompositionText())
    ConfirmCompositionText();
  selection_start_ = GetSafePosition(range.start());
  cursor_pos_ = GetSafePosition(range.end());
}

void TextfieldViewsModel::SelectAll() {
  if (HasCompositionText())
    ConfirmCompositionText();
  // SelectAll selects towards the end.
  cursor_pos_ = text_.length();
  selection_start_ = 0;
}

void TextfieldViewsModel::SelectWord() {
  if (HasCompositionText())
    ConfirmCompositionText();
  // First we setup selection_start_ and cursor_pos_. There are so many cases
  // because we try to emulate what select-word looks like in a gtk textfield.
  // See associated testcase for different cases.
  if (cursor_pos_ > 0 && cursor_pos_ < text_.length()) {
    if (isalnum(text_[cursor_pos_])) {
      selection_start_ = cursor_pos_;
      cursor_pos_++;
    } else
      selection_start_ = cursor_pos_ - 1;
  } else if (cursor_pos_ == 0) {
    selection_start_ = cursor_pos_;
    if (text_.length() > 0)
      cursor_pos_++;
  } else {
    selection_start_ = cursor_pos_ - 1;
  }

  // Now we move selection_start_ to beginning of selection. Selection boundary
  // is defined as the position where we have alpha-num character on one side
  // and non-alpha-num char on the other side.
  for (; selection_start_ > 0; selection_start_--) {
    if (IsPositionAtWordSelectionBoundary(selection_start_))
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
  if (HasCompositionText())
    ConfirmCompositionText();
  selection_start_ = cursor_pos_;
}

bool TextfieldViewsModel::CanUndo() {
  return edit_history_.size() && current_edit_ != edit_history_.end();
}

bool TextfieldViewsModel::CanRedo() {
  if (!edit_history_.size())
    return false;
  // There is no redo iff the current edit is the last element
  // in the history.
  EditHistory::iterator iter = current_edit_;
  return iter == edit_history_.end() || // at the top.
      ++iter != edit_history_.end();
}

bool TextfieldViewsModel::Undo() {
  if (!CanUndo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText())  // safe guard for release build.
    ClearCompositionText();

  string16 old = text_;
  update_history_ = false;
  (*current_edit_)->Commit();
  (*current_edit_)->Undo(this);
  update_history_ = true;

  if (current_edit_ == edit_history_.begin())
    current_edit_ = edit_history_.end();
  else
    current_edit_--;
  return old != text_;
}

bool TextfieldViewsModel::Redo() {
  if (!CanRedo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText()) // safe guard for release build.
    ClearCompositionText();

  if (current_edit_ == edit_history_.end())
    current_edit_ = edit_history_.begin();
  else
    current_edit_ ++;
  string16 old = text_;
  update_history_ = false;
  (*current_edit_)->Redo(this);
  update_history_ = true;
  return old != text_;
}

bool TextfieldViewsModel::Cut() {
  if (!HasCompositionText() && HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
    DeleteSelection(true);
    return true;
  }
  return false;
}

void TextfieldViewsModel::Copy() {
  if (!HasCompositionText() && HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
  }
}

bool TextfieldViewsModel::Paste() {
  string16 result;
  views::ViewsDelegate::views_delegate->GetClipboard()
      ->ReadText(ui::Clipboard::BUFFER_STANDARD, &result);
  if (!result.empty()) {
    InsertTextInternal(result, false);
    return true;
  }
  return false;
}

bool TextfieldViewsModel::HasSelection() const {
  return selection_start_ != cursor_pos_;
}

void TextfieldViewsModel::DeleteSelection(bool record_edit_history) {
  DCHECK(!HasCompositionText());
  DCHECK(HasSelection());
  size_t n = std::abs(static_cast<long>(cursor_pos_ - selection_start_));
  size_t begin = std::min(cursor_pos_, selection_start_);
  if (record_edit_history)
    RecordDelete(cursor_pos_, selection_start_ - cursor_pos_, false);
  text_.erase(begin, n);
  cursor_pos_ = begin;
  ClearSelection();
}

string16 TextfieldViewsModel::GetTextFromRange(const ui::Range& range) const {
  if (range.IsValid() && range.GetMin() < text_.length())
    return text_.substr(range.GetMin(), range.length());
  return string16();
}

void TextfieldViewsModel::GetTextRange(ui::Range* range) const {
  *range = ui::Range(0, text_.length());
}

void TextfieldViewsModel::SetCompositionText(
    const ui::CompositionText& composition) {
  if (HasCompositionText())
    ClearCompositionText();
  else if (HasSelection())
    DeleteSelection(true);

  if (composition.text.empty())
    return;

  size_t length = composition.text.length();
  text_.insert(cursor_pos_, composition.text);
  composition_start_ = cursor_pos_;
  composition_end_ = composition_start_ + length;
  composition_underlines_ = composition.underlines;

  if (composition.selection.IsValid()) {
    selection_start_ =
        std::min(composition_start_ + composition.selection.start(),
                 composition_end_);
    cursor_pos_ =
        std::min(composition_start_ + composition.selection.end(),
                 composition_end_);
  } else {
    cursor_pos_ = composition_end_;
    ClearSelection();
  }
}

void TextfieldViewsModel::ConfirmCompositionText() {
  DCHECK(HasCompositionText());
  string16 new_text =
      text_.substr(composition_start_, composition_end_ - composition_start_);
  // TODO(oshima): current behavior on ChromeOS is a bit weird and not
  // sure exactly how this should work. Find out and fix if necessary.
  AddEditHistory(new InsertEdit(false, composition_start_, new_text));
  cursor_pos_ = composition_end_;
  composition_start_ = composition_end_ = string16::npos;
  composition_underlines_.clear();
  ClearSelection();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldViewsModel::ClearCompositionText() {
  DCHECK(HasCompositionText());
  text_.erase(composition_start_, composition_end_ - composition_start_);
  cursor_pos_ = composition_start_;
  composition_start_ = composition_end_ = string16::npos;
  composition_underlines_.clear();
  ClearSelection();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldViewsModel::GetCompositionTextRange(ui::Range* range) const {
  if (HasCompositionText())
    *range = ui::Range(composition_start_, composition_end_);
  else
    *range = ui::Range::InvalidRange();
}

bool TextfieldViewsModel::HasCompositionText() const {
  return composition_start_ != composition_end_;
}

/////////////////////////////////////////////////////////////////
// TextfieldViewsModel: private

string16 TextfieldViewsModel::GetVisibleText(size_t begin, size_t end) const {
  DCHECK(end >= begin);
  if (is_password_)
    return string16(end - begin, '*');
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

void TextfieldViewsModel::InsertTextInternal(const string16& text,
                                             bool mergeable) {
  // TODO(oshima): Simplify the implementation by using an edit TO
  // modify the text here as well, instead of create an edit AND
  // modify the text.
  if (HasCompositionText()) {
    ClearCompositionText();
    RecordInsert(text, mergeable);
  } else if (HasSelection()) {
    RecordReplace(text, mergeable);
    DeleteSelection(false);
  } else {
    RecordInsert(text, mergeable);
  }
  text_.insert(cursor_pos_, text);
  cursor_pos_ += text.size();
  ClearSelection();
}

void TextfieldViewsModel::ReplaceTextInternal(const string16& text,
                                              bool mergeable) {
  if (HasCompositionText())
    ClearCompositionText();
  else if (!HasSelection())
    SelectRange(ui::Range(cursor_pos_ + text.length(), cursor_pos_));
  // Edit history is recorded in InsertText.
  InsertTextInternal(text, mergeable);
}

void TextfieldViewsModel::ClearEditHistory() {
  STLDeleteContainerPointers(edit_history_.begin(),
                             edit_history_.end());
  edit_history_.clear();
  current_edit_ = edit_history_.end();
}

void TextfieldViewsModel::ClearRedoHistory() {
  if (edit_history_.begin() == edit_history_.end())
    return;
  if (current_edit_ == edit_history_.end()) {
    ClearEditHistory();
    return;
  }
  EditHistory::iterator delete_start = current_edit_;
  delete_start++;
  STLDeleteContainerPointers(delete_start,
                             edit_history_.end());
  edit_history_.erase(delete_start, edit_history_.end());
}

void TextfieldViewsModel::RecordDelete(size_t cursor, int size,
                                       bool mergeable) {
  if (!update_history_)
    return;
  ClearRedoHistory();
  int c = std::min(cursor, cursor + size);
  const string16 text = text_.substr(c, std::abs(size));
  AddEditHistory(new DeleteEdit(mergeable, cursor, size, text));
}

void TextfieldViewsModel::RecordReplace(const string16& text,
                                        bool mergeable) {
  if (!update_history_)
    return;
  ClearRedoHistory();
  AddEditHistory(new ReplaceEdit(mergeable,
                                 cursor_pos_,
                                 selection_start_ - cursor_pos_,
                                 GetSelectedText(),
                                 text));
}

void TextfieldViewsModel::RecordInsert(const string16& text,
                                       bool mergeable) {
  if (!update_history_)
    return;
  ClearRedoHistory();
  AddEditHistory(new InsertEdit(mergeable, cursor_pos_, text));
}

void TextfieldViewsModel::AddEditHistory(internal::Edit* edit) {
  DCHECK(update_history_);
  if (current_edit_ != edit_history_.end() && (*current_edit_)->Merge(edit)) {
    // If a current edit exists and has been merged with a new edit,
    // delete that edit.
    delete edit;
    return;
  }
  edit_history_.push_back(edit);
  if (current_edit_ == edit_history_.end()) {
    // If there is no redoable edit, this is the 1st edit because
    // RedoHistory has been already deleted.
    DCHECK_EQ(1u, edit_history_.size());
    current_edit_ = edit_history_.begin();
  } else {
    current_edit_++;
  }
}

}  // namespace views
