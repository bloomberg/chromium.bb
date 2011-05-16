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
class Edit {
 public:
  enum Type {
    INSERT_EDIT,
    DELETE_EDIT,
    REPLACE_EDIT
  };

  virtual ~Edit() {
  }

  // Revert the change made by this edit in |model|.
  void Undo(TextfieldViewsModel* model) {
    size_t old_cursor = delete_backward_ ? old_text_end() : old_text_start_;
    model->ModifyText(new_text_start_, new_text_end(),
                      old_text_, old_text_start_,
                      old_cursor);
  }

  // Apply the change of this edit to the |model|.
  void Redo(TextfieldViewsModel* model) {
    model->ModifyText(old_text_start_, old_text_end(),
                      new_text_, new_text_start_,
                      new_text_end());
  }

  // Try to merge the edit into this edit. Returns true if merge was
  // successful, or false otherwise. Merged edit will be deleted after
  // redo and should not be reused.
  bool Merge(Edit* edit) {
    return mergeable_ && edit->mergeable() && DoMerge(edit);
  }

  // Commits the edit and marks as un-mergeable.
  void Commit() { mergeable_ = false; }

 private:
  friend class InsertEdit;
  friend class ReplaceEdit;
  friend class DeleteEdit;

  Edit(Type type,
       bool mergeable,
       string16 old_text,
       size_t old_text_start,
       bool delete_backward,
       string16 new_text,
       size_t new_text_start)
      : type_(type),
        mergeable_(mergeable),
        old_text_(old_text),
        old_text_start_(old_text_start),
        delete_backward_(delete_backward),
        new_text_(new_text),
        new_text_start_(new_text_start) {
  }

  // A template method pattern that provides specific merge
  // implementation for each type of edit.
  virtual bool DoMerge(Edit* edit) = 0;

  Type type() { return type_; }

  // Can this edit be merged?
  bool mergeable() { return mergeable_; }

  // Returns the end index of the |old_text_|.
  size_t old_text_end() { return old_text_start_ + old_text_.length(); }

  // Returns the end index of the |new_text_|.
  size_t new_text_end() { return new_text_start_ + new_text_.length(); }

  Type type_;

  // True if the edit can be marged.
  bool mergeable_;
  // Deleted text by this edit.
  string16 old_text_;
  // The index of |old_text_|.
  size_t old_text_start_;
  // True if the deletion is made backward.
  bool delete_backward_;
  // Added text.
  string16 new_text_;
  // The index of |new_text_|
  size_t new_text_start_;

  DISALLOW_COPY_AND_ASSIGN(Edit);
};

class InsertEdit : public Edit {
 public:
  InsertEdit(bool mergeable, const string16& new_text, size_t at)
      : Edit(INSERT_EDIT, mergeable, string16(), at, false, new_text, at) {
  }

  // Edit implementation.
  virtual bool DoMerge(Edit* edit) OVERRIDE {
    if (edit->type() != INSERT_EDIT || new_text_end() != edit->new_text_start_)
      return false;
    // If continuous edit, merge it.
    // TODO(oshima): gtk splits edits between whitespace. Find out what
    // we want to here and implement if necessary.
    new_text_ += edit->new_text_;
    return true;
  }
};

class ReplaceEdit : public Edit {
 public:
  ReplaceEdit(bool mergeable,
              const string16& old_text,
              size_t old_text_start,
              bool backward,
              const string16& new_text,
              size_t new_text_start)
      : Edit(REPLACE_EDIT, mergeable,
             old_text,
             old_text_start,
             backward,
             new_text,
             new_text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(Edit* edit) OVERRIDE {
    if (edit->type() == DELETE_EDIT ||
        new_text_end() != edit->old_text_start_ ||
        edit->old_text_start_ != edit->new_text_start_)
      return false;
    old_text_ += edit->old_text_;
    new_text_ += edit->new_text_;
    return true;
  }
};

class DeleteEdit : public Edit {
 public:
  DeleteEdit(bool mergeable,
             const string16& text,
             size_t text_start,
             bool backward)
      : Edit(DELETE_EDIT, mergeable,
             text,
             text_start,
             backward,
             string16(),
             text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(Edit* edit) OVERRIDE {
    if (edit->type() != DELETE_EDIT)
      return false;

    if (delete_backward_) {
      // backspace can be merged only with backspace at the
      // same position.
      if (!edit->delete_backward_ || old_text_start_ != edit->old_text_end())
        return false;
      old_text_start_ = edit->old_text_start_;
      old_text_ = edit->old_text_ + old_text_;
    } else {
      // delete can be merged only with delete at the same
      // position.
      if (edit->delete_backward_ || old_text_start_ != edit->old_text_start_)
        return false;
      old_text_ += edit->old_text_;
    }
    return true;
  }
};

}  // namespace internal

using internal::Edit;
using internal::DeleteEdit;
using internal::InsertEdit;
using internal::ReplaceEdit;

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
      current_edit_(edit_history_.end()) {
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
    DeleteSelection();
    return true;
  }
  if (text_.length() > cursor_pos_) {
    ExecuteAndRecordDelete(cursor_pos_, cursor_pos_ + 1, true);
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
    DeleteSelection();
    return true;
  }
  if (cursor_pos_ > 0) {
    ExecuteAndRecordDelete(cursor_pos_, cursor_pos_ - 1, true);
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
  base::i18n::BreakIterator iter(&text_, base::i18n::BreakIterator::BREAK_WORD);
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
  base::i18n::BreakIterator iter(&text_, base::i18n::BreakIterator::BREAK_WORD);
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
  (*current_edit_)->Commit();
  (*current_edit_)->Undo(this);

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
  (*current_edit_)->Redo(this);
  return old != text_;
}

bool TextfieldViewsModel::Cut() {
  if (!HasCompositionText() && HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
    DeleteSelection();
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

void TextfieldViewsModel::DeleteSelection() {
  DCHECK(!HasCompositionText());
  DCHECK(HasSelection());
  ExecuteAndRecordDelete(selection_start_, cursor_pos_, false);
}

void TextfieldViewsModel::DeleteSelectionAndInsertTextAt(
    const string16& text, size_t position) {
  if (HasCompositionText())
    ClearCompositionText();
  ExecuteAndRecordReplaceAt(text, position, false);
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
    DeleteSelection();

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
  AddOrMergeEditHistory(new InsertEdit(false, new_text, composition_start_));
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
  if (HasCompositionText()) {
    ClearCompositionText();
    ExecuteAndRecordInsert(text, mergeable);
  } else if (HasSelection()) {
    ExecuteAndRecordReplace(text, mergeable);
  } else {
    ExecuteAndRecordInsert(text, mergeable);
  }
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

void TextfieldViewsModel::ExecuteAndRecordDelete(size_t from,
                                                 size_t to,
                                                 bool mergeable) {
  size_t old_text_start = std::min(from, to);
  const string16 text = text_.substr(old_text_start,
                                     std::abs(static_cast<long>(from - to)));
  Edit* edit = new DeleteEdit(mergeable,
                              text,
                              old_text_start,
                              (from > to));
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

void TextfieldViewsModel::ExecuteAndRecordReplace(const string16& text,
                                                  bool mergeable) {
  size_t at = std::min(cursor_pos_, selection_start_);
  ExecuteAndRecordReplaceAt(text, at, mergeable);
}

void TextfieldViewsModel::ExecuteAndRecordReplaceAt(const string16& text,
                                                    size_t at,
                                                    bool mergeable) {
  size_t text_start = std::min(cursor_pos_, selection_start_);
  Edit* edit = new ReplaceEdit(mergeable,
                               GetSelectedText(),
                               text_start,
                               selection_start_ > cursor_pos_,
                               text,
                               at);
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

void TextfieldViewsModel::ExecuteAndRecordInsert(const string16& text,
                                                 bool mergeable) {
  Edit* edit = new InsertEdit(mergeable, text, cursor_pos_);
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

bool TextfieldViewsModel::AddOrMergeEditHistory(Edit* edit) {
  ClearRedoHistory();

  if (current_edit_ != edit_history_.end() && (*current_edit_)->Merge(edit)) {
    // If a current edit exists and has been merged with a new edit,
    // don't add to the history, and return true to delete |edit| after
    // redo.
    return true;
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
  return false;
}

void TextfieldViewsModel::ModifyText(size_t delete_from,
                                     size_t delete_to,
                                     const string16& new_text,
                                     size_t new_text_insert_at,
                                     size_t new_cursor_pos) {
  DCHECK_LE(delete_from, delete_to);
  if (delete_from != delete_to)
    text_.erase(delete_from, delete_to - delete_from);
  if (!new_text.empty())
    text_.insert(new_text_insert_at, new_text);
  cursor_pos_ = new_cursor_pos;
  ClearSelection();
  // TODO(oshima): mac selects the text that is just undone (but gtk doesn't).
  // This looks fine feature and we may want to do the same.
}

}  // namespace views
