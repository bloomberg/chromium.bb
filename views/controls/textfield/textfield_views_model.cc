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
#include "views/controls/textfield/text_style.h"
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
    model->ModifyText(new_text_start_, new_text_end(),
                      old_text_, old_text_start_,
                      old_cursor_pos_);
  }

  // Apply the change of this edit to the |model|.
  void Redo(TextfieldViewsModel* model) {
    model->ModifyText(old_text_start_, old_text_end(),
                      new_text_, new_text_start_,
                      new_cursor_pos_);
  }

  // Try to merge the |edit| into this edit. Returns true if merge was
  // successful, or false otherwise. Merged edit will be deleted after
  // redo and should not be reused.
  bool Merge(const Edit* edit) {
    if (edit->merge_with_previous()) {
      MergeSet(edit);
      return true;
    }
    return mergeable() && edit->mergeable() && DoMerge(edit);
  }

  // Commits the edit and marks as un-mergeable.
  void Commit() { merge_type_ = DO_NOT_MERGE; }

 private:
  friend class InsertEdit;
  friend class ReplaceEdit;
  friend class DeleteEdit;

  Edit(Type type,
       MergeType merge_type,
       size_t old_cursor_pos,
       string16 old_text,
       size_t old_text_start,
       bool delete_backward,
       size_t new_cursor_pos,
       string16 new_text,
       size_t new_text_start)
      : type_(type),
        merge_type_(merge_type),
        old_cursor_pos_(old_cursor_pos),
        old_text_(old_text),
        old_text_start_(old_text_start),
        delete_backward_(delete_backward),
        new_cursor_pos_(new_cursor_pos),
        new_text_(new_text),
        new_text_start_(new_text_start) {
  }

  // A template method pattern that provides specific merge
  // implementation for each type of edit.
  virtual bool DoMerge(const Edit* edit) = 0;

  Type type() const { return type_; }

  // Can this edit be merged?
  bool mergeable() const { return merge_type_ == MERGEABLE; }

  // Should this edit be forcibly merged with the previous edit?
  bool merge_with_previous() const {
    return merge_type_ == MERGE_WITH_PREVIOUS;
  }

  // Returns the end index of the |old_text_|.
  size_t old_text_end() const { return old_text_start_ + old_text_.length(); }

  // Returns the end index of the |new_text_|.
  size_t new_text_end() const { return new_text_start_ + new_text_.length(); }

  // Merge the Set edit into the current edit. This is a special case to
  // handle an omnibox setting autocomplete string after new character is
  // typed in.
  void MergeSet(const Edit* edit) {
    CHECK_EQ(REPLACE_EDIT, edit->type_);
    CHECK_EQ(0U, edit->old_text_start_);
    CHECK_EQ(0U, edit->new_text_start_);
    string16 old_text = edit->old_text_;
    old_text.erase(new_text_start_, new_text_.length());
    old_text.insert(old_text_start_, old_text_);
    // SetText() replaces entire text. Set |old_text_| to the entire
    // replaced text with |this| edit undone.
    old_text_ = old_text;
    old_text_start_ = edit->old_text_start_;
    delete_backward_ = false;

    new_text_ = edit->new_text_;
    new_text_start_ = edit->new_text_start_;
    merge_type_ = DO_NOT_MERGE;
  }

  Type type_;

  // True if the edit can be marged.
  MergeType merge_type_;
  // Old cursor position.
  size_t old_cursor_pos_;
  // Deleted text by this edit.
  string16 old_text_;
  // The index of |old_text_|.
  size_t old_text_start_;
  // True if the deletion is made backward.
  bool delete_backward_;
  // New cursor position.
  size_t new_cursor_pos_;
  // Added text.
  string16 new_text_;
  // The index of |new_text_|
  size_t new_text_start_;

  DISALLOW_COPY_AND_ASSIGN(Edit);
};

class InsertEdit : public Edit {
 public:
  InsertEdit(bool mergeable, const string16& new_text, size_t at)
      : Edit(INSERT_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             at  /* old cursor */,
             string16(),
             at,
             false  /* N/A */,
             at + new_text.length()  /* new cursor */,
             new_text,
             at) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() != INSERT_EDIT || new_text_end() != edit->new_text_start_)
      return false;
    // If continuous edit, merge it.
    // TODO(oshima): gtk splits edits between whitespace. Find out what
    // we want to here and implement if necessary.
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class ReplaceEdit : public Edit {
 public:
  ReplaceEdit(MergeType merge_type,
              const string16& old_text,
              size_t old_cursor_pos,
              size_t old_text_start,
              bool backward,
              size_t new_cursor_pos,
              const string16& new_text,
              size_t new_text_start)
      : Edit(REPLACE_EDIT, merge_type,
             old_cursor_pos,
             old_text,
             old_text_start,
             backward,
             new_cursor_pos,
             new_text,
             new_text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() == DELETE_EDIT ||
        new_text_end() != edit->old_text_start_ ||
        edit->old_text_start_ != edit->new_text_start_)
      return false;
    old_text_ += edit->old_text_;
    new_text_ += edit->new_text_;
    new_cursor_pos_ = edit->new_cursor_pos_;
    return true;
  }
};

class DeleteEdit : public Edit {
 public:
  DeleteEdit(bool mergeable,
             const string16& text,
             size_t text_start,
             bool backward)
      : Edit(DELETE_EDIT,
             mergeable ? MERGEABLE : DO_NOT_MERGE,
             (backward ? text_start + text.length() : text_start),
             text,
             text_start,
             backward,
             text_start,
             string16(),
             text_start) {
  }

  // Edit implementation.
  virtual bool DoMerge(const Edit* edit) OVERRIDE {
    if (edit->type() != DELETE_EDIT)
      return false;

    if (delete_backward_) {
      // backspace can be merged only with backspace at the
      // same position.
      if (!edit->delete_backward_ || old_text_start_ != edit->old_text_end())
        return false;
      old_text_start_ = edit->old_text_start_;
      old_text_ = edit->old_text_ + old_text_;
      new_cursor_pos_ = edit->new_cursor_pos_;
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

struct TextStyleRange {
  TextStyleRange(const TextStyle* s, size_t start, size_t end)
      : style(s),
        range(start, end) {
  }
  TextStyleRange(const TextStyle* s, const ui::Range& r)
      : style(s),
        range(r) {
  }
  const TextStyle *style;
  ui::Range range;
};

}  // namespace internal

namespace {

using views::internal::TextStyleRange;

static bool TextStyleRangeComparator(const TextStyleRange* i,
                                     const TextStyleRange* j) {
  return i->range.start() < j->range.start();
}

#ifndef NDEBUG
// A test function to check TextStyleRanges' invariant condition:
// "no overlapping range".
bool CheckInvariant(const TextStyleRanges* style_ranges) {
  TextStyleRanges copy = *style_ranges;
  std::sort(copy.begin(), copy.end(), TextStyleRangeComparator);

  for (TextStyleRanges::size_type i = 0; i < copy.size() - 1; i++) {
    ui::Range& former = copy[i]->range;
    ui::Range& latter = copy[i + 1]->range;
    if (former.is_empty()) {
      LOG(WARNING) << "Empty range at " << i << " :" << former;
      return false;
    }
    if (!former.IsValid()) {
      LOG(WARNING) << "Invalid range at " << i << " :" << former;
      return false;
    }
    if (former.GetMax() > latter.GetMin()) {
      LOG(WARNING) <<
          "Sorting error. former:" << former << " latter:" << latter;
      return false;
    }
    if (former.Intersects(latter)) {
      LOG(ERROR) << "overlapping style range found: former=" << former
                 << ", latter=" << latter;
      return false;
    }
  }
  if ((*copy.rbegin())->range.is_empty()) {
    LOG(WARNING) << "Empty range at end";
    return false;
  }
  if (!(*copy.rbegin())->range.IsValid()) {
    LOG(WARNING) << "Invalid range at end";
    return false;
  }
  return true;
}
#endif

void InsertStyle(TextStyleRanges* style_ranges,
                 TextStyleRange* text_style_range) {
  const ui::Range& range = text_style_range->range;
  if (range.is_empty() || !range.IsValid()) {
    delete text_style_range;
    return;
  }
  CHECK(!range.is_reversed());

  // Invariant condition: all items in the range has no overlaps.
  TextStyleRanges::size_type index = 0;
  while (index < style_ranges->size()) {
    TextStyleRange* current = (*style_ranges)[index];
    if (range.Contains(current->range)) {
      style_ranges->erase(style_ranges->begin() + index);
      delete current;
      continue;
    } else if (current->range.Contains(range) &&
               range.start() != current->range.start() &&
               range.end() != current->range.end()) {
      // Split current style into two styles.
      style_ranges->push_back(
          new TextStyleRange(current->style,
                             range.GetMax(), current->range.GetMax()));
      current->range.set_end(range.GetMin());
    } else if (range.Intersects(current->range)) {
      if (current->range.GetMax() <= range.GetMax()) {
        current->range.set_end(range.GetMin());
      } else {
        current->range.set_start(range.GetMax());
      }
    } else {
      // No change needed. Pass it through.
    }
    index ++;
  }
  // Add the new range at the end.
  style_ranges->push_back(text_style_range);
#ifndef NDEBUG
  DCHECK(CheckInvariant(style_ranges));
#endif
}

}  // namespace

using internal::Edit;
using internal::DeleteEdit;
using internal::InsertEdit;
using internal::ReplaceEdit;
using internal::MergeType;
using internal::DO_NOT_MERGE;
using internal::MERGE_WITH_PREVIOUS;
using internal::MERGEABLE;

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
      sort_style_ranges_(false) {
}

TextfieldViewsModel::~TextfieldViewsModel() {
  ClearEditHistory();
  ClearComposition();
  ClearAllTextStyles();
  TextStyles::iterator begin = text_styles_.begin();
  TextStyles::iterator end = text_styles_.end();
  while (begin != end) {
    TextStyles::iterator temp = begin;
    ++begin;
    delete *temp;
  }
}

void TextfieldViewsModel::GetFragments(TextFragments* fragments) {
  static const TextStyle* kNormalStyle = new TextStyle();

  if (sort_style_ranges_) {
    sort_style_ranges_ = false;
    std::sort(style_ranges_.begin(), style_ranges_.end(),
              TextStyleRangeComparator);
  }

  // If a user is compositing text, use composition's style.
  // TODO(oshima): ask suzhe for expected behavior.
  const TextStyleRanges& ranges = composition_style_ranges_.size() > 0 ?
      composition_style_ranges_ : style_ranges_;
  TextStyleRanges::const_iterator next_ = ranges.begin();

  DCHECK(fragments);
  fragments->clear();
  size_t current = 0;
  size_t end = text_.length();
  while(next_ != ranges.end()) {
    const TextStyleRange* text_style_range = *next_++;
    const ui::Range& range = text_style_range->range;
    const TextStyle* style = text_style_range->style;

    DCHECK(!range.is_empty());
    DCHECK(range.IsValid());
    if (range.is_empty() || !range.IsValid())
      continue;

    size_t start = std::min(range.start(), end);

    if (start == end)  // Exit loop if it reached the end.
      break;
    else if (current < start)  // Fill the gap to next style with normal text.
      fragments->push_back(TextFragment(current, start, kNormalStyle));

    current = std::min(range.end(), end);
    fragments->push_back(TextFragment(start, current, style));
  }
  // If there is any text left add it as normal text.
  if (current != end)
    fragments->push_back(TextFragment(current, end, kNormalStyle));
}

bool TextfieldViewsModel::SetText(const string16& text) {
  bool changed = false;
  if (HasCompositionText()) {
    ConfirmCompositionText();
    changed = true;
  }
  if (text_ != text) {
    if (changed)  // No need to remember composition.
      Undo();
    size_t old_cursor = cursor_pos_;
    size_t new_cursor = old_cursor > text.length() ? text.length() : old_cursor;
    SelectAll();
    // If there is a composition text, don't merge with previous edit.
    // Otherwise, force merge the edits.
    ExecuteAndRecordReplace(
        changed ? DO_NOT_MERGE : MERGE_WITH_PREVIOUS,
        old_cursor,
        new_cursor,
        text,
        0U);
    cursor_pos_ = new_cursor;
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
    CancelCompositionText();
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
    CancelCompositionText();
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
  base::i18n::BreakIterator iter(text_, base::i18n::BreakIterator::BREAK_WORD);
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
  base::i18n::BreakIterator iter(text_, base::i18n::BreakIterator::BREAK_WORD);
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

gfx::Rect TextfieldViewsModel::GetSelectionBounds(const gfx::Font& font) const {
  if (!HasSelection())
    return gfx::Rect();
  size_t start = std::min(selection_start_, cursor_pos_);
  size_t end = std::max(selection_start_, cursor_pos_);
  int start_x = font.GetStringWidth(text_.substr(0, start));
  int end_x = font.GetStringWidth(text_.substr(0, end));
  return gfx::Rect(start_x, 0, end_x - start_x, font.GetHeight());
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
    CancelCompositionText();

  string16 old = text_;
  size_t old_cursor = cursor_pos_;
  (*current_edit_)->Commit();
  (*current_edit_)->Undo(this);

  if (current_edit_ == edit_history_.begin())
    current_edit_ = edit_history_.end();
  else
    current_edit_--;
  return old != text_ || old_cursor != cursor_pos_;
}

bool TextfieldViewsModel::Redo() {
  if (!CanRedo())
    return false;
  DCHECK(!HasCompositionText());
  if (HasCompositionText()) // safe guard for release build.
    CancelCompositionText();

  if (current_edit_ == edit_history_.end())
    current_edit_ = edit_history_.begin();
  else
    current_edit_ ++;
  string16 old = text_;
  size_t old_cursor = cursor_pos_;
  (*current_edit_)->Redo(this);
  return old != text_ || old_cursor != cursor_pos_;
}

bool TextfieldViewsModel::Cut() {
  if (!HasCompositionText() && HasSelection()) {
    ui::ScopedClipboardWriter(views::ViewsDelegate::views_delegate
        ->GetClipboard()).WriteText(GetSelectedText());
    // A trick to let undo/redo handle cursor correctly.
    // Undoing CUT moves the cursor to the end of the change rather
    // than beginning, unlike Delete/Backspace.
    // TODO(oshima): Change Delete/Backspace to use DeleteSelection,
    // update DeleteEdit and remove this trick.
    std::swap(cursor_pos_, selection_start_);
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
    CancelCompositionText();
  ExecuteAndRecordReplace(DO_NOT_MERGE,
                          cursor_pos_,
                          position + text.length(),
                          text,
                          position);
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
  static const TextStyle* composition_style = CreateUnderlineStyle();

  if (HasCompositionText())
    CancelCompositionText();
  else if (HasSelection())
    DeleteSelection();

  if (composition.text.empty())
    return;

  size_t length = composition.text.length();
  text_.insert(cursor_pos_, composition.text);
  composition_start_ = cursor_pos_;
  composition_end_ = composition_start_ + length;
  for (ui::CompositionUnderlines::const_iterator iter =
           composition.underlines.begin();
       iter != composition.underlines.end();
       iter++) {
    size_t start = composition_start_ + iter->start_offset;
    size_t end = composition_start_ + iter->end_offset;
    InsertStyle(&composition_style_ranges_,
                new TextStyleRange(composition_style, start, end));
  }
  std::sort(composition_style_ranges_.begin(),
            composition_style_ranges_.end(), TextStyleRangeComparator);

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
  ClearComposition();
  ClearSelection();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldViewsModel::CancelCompositionText() {
  DCHECK(HasCompositionText());
  text_.erase(composition_start_, composition_end_ - composition_start_);
  cursor_pos_ = composition_start_;
  ClearComposition();
  ClearSelection();
  if (delegate_)
    delegate_->OnCompositionTextConfirmedOrCleared();
}

void TextfieldViewsModel::ClearComposition() {
  composition_start_ = composition_end_ = string16::npos;
  STLDeleteContainerPointers(composition_style_ranges_.begin(),
                             composition_style_ranges_.end());
  composition_style_ranges_.clear();
}

void TextfieldViewsModel::ApplyTextStyle(const TextStyle* style,
                                         const ui::Range& range) {
  TextStyleRange* new_text_style_range = range.is_reversed() ?
      new TextStyleRange(style, ui::Range(range.end(), range.start())) :
      new TextStyleRange(style, range);
  InsertStyle(&style_ranges_, new_text_style_range);
  sort_style_ranges_ = true;
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

TextStyle* TextfieldViewsModel::CreateTextStyle() {
  TextStyle* style = new TextStyle();
  text_styles_.push_back(style);
  return style;
}

void TextfieldViewsModel::ClearAllTextStyles() {
  STLDeleteContainerPointers(style_ranges_.begin(), style_ranges_.end());
  style_ranges_.clear();
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
    CancelCompositionText();
    ExecuteAndRecordInsert(text, mergeable);
  } else if (HasSelection()) {
    ExecuteAndRecordReplaceSelection(mergeable ? MERGEABLE : DO_NOT_MERGE,
                                     text);
  } else {
    ExecuteAndRecordInsert(text, mergeable);
  }
}

void TextfieldViewsModel::ReplaceTextInternal(const string16& text,
                                              bool mergeable) {
  if (HasCompositionText())
    CancelCompositionText();
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
  STLDeleteContainerPointers(delete_start, edit_history_.end());
  edit_history_.erase(delete_start, edit_history_.end());
}

void TextfieldViewsModel::ExecuteAndRecordDelete(size_t from,
                                                 size_t to,
                                                 bool mergeable) {
  size_t old_text_start = std::min(from, to);
  const string16 text = text_.substr(old_text_start,
                                     std::abs(static_cast<long>(from - to)));
  bool backward = from > to;
  Edit* edit = new DeleteEdit(mergeable, text, old_text_start, backward);
  bool delete_edit = AddOrMergeEditHistory(edit);
  edit->Redo(this);
  if (delete_edit)
    delete edit;
}

void TextfieldViewsModel::ExecuteAndRecordReplaceSelection(
    MergeType merge_type, const string16& new_text) {
  size_t new_text_start = std::min(cursor_pos_, selection_start_);
  size_t new_cursor_pos = new_text_start + new_text.length();
  ExecuteAndRecordReplace(merge_type,
                          cursor_pos_,
                          new_cursor_pos,
                          new_text,
                          new_text_start);
}

void TextfieldViewsModel::ExecuteAndRecordReplace(MergeType merge_type,
                                                  size_t old_cursor_pos,
                                                  size_t new_cursor_pos,
                                                  const string16& new_text,
                                                  size_t new_text_start) {
  size_t old_text_start = std::min(cursor_pos_, selection_start_);
  bool backward = selection_start_ > cursor_pos_;
  Edit* edit = new ReplaceEdit(merge_type,
                               GetSelectedText(),
                               old_cursor_pos,
                               old_text_start,
                               backward,
                               new_cursor_pos,
                               new_text,
                               new_text_start);
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

// static
TextStyle* TextfieldViewsModel::CreateUnderlineStyle() {
  TextStyle* style = new TextStyle();
  style->set_underline(true);
  return style;
}

}  // namespace views
