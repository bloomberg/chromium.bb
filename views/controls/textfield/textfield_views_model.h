// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#pragma once

#include <list>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/composition_text.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Font;
}  // namespace gfx

namespace ui {
class Range;
}  // namespace ui

namespace views {

namespace internal {
// Internal Edit class that keeps track of edits for undo/redo.
class Edit;
}  // namespace internal

// A model that represents a text content for TextfieldViews.
// It supports editing, selection and cursor manipulation.
class TextfieldViewsModel {
 public:

  // Delegate interface implemented by the textfield view class to provided
  // additional functionalities required by the model.
  class Delegate {
   public:
    // Called when the current composition text is confirmed or cleared.
    virtual void OnCompositionTextConfirmedOrCleared() = 0;

   protected:
    virtual ~Delegate();
  };

  explicit TextfieldViewsModel(Delegate* delegate);
  virtual ~TextfieldViewsModel();

  // Text fragment info. Used to draw selected text.
  // We may replace this with TextAttribute class
  // in the future to support multi-color text
  // for omnibox.
  struct TextFragment {
    TextFragment(size_t s, size_t e, bool sel, bool u)
        : start(s), end(e), selected(sel), underline(u) {
    }
    // The start and end position of text fragment.
    size_t start, end;
    // True if the text is selected.
    bool selected;
    // True if the text has underline.
    // TODO(suzhe): support underline color and thick style.
    bool underline;
  };
  typedef std::vector<TextFragment> TextFragments;

  // Gets the text element info.
  void GetFragments(TextFragments* elements) const;

  void set_is_password(bool is_password) {
    is_password_ = is_password;
  }
  const string16& text() const { return text_; }

  // Edit related methods.

  // Sest the text. Returns true if the text has been modified.  The
  // current composition text will be confirmed first.  Setting
  // the same text will not add edit history because it's not user
  // visible change nor user-initiated change. This allow a client
  // code to set the same text multiple times without worrying about
  // messing edit history.
  bool SetText(const string16& text);

  // Inserts given |text| at the current cursor position.
  // The current composition text will be cleared.
  void InsertText(const string16& text) {
    InsertTextInternal(text, false);
  }

  // Inserts a character at the current cursor position.
  void InsertChar(char16 c) {
    InsertTextInternal(string16(&c, 1), true);
  }

  // Replaces characters at the current position with characters in given text.
  // The current composition text will be cleared.
  void ReplaceText(const string16& text) {
    ReplaceTextInternal(text, false);
  }

  // Replaces the char at the current position with given character.
  void ReplaceChar(char16 c) {
    ReplaceTextInternal(string16(&c, 1), true);
  }

  // Appends the text.
  // The current composition text will be confirmed.
  void Append(const string16& text);

  // Deletes the first character after the current cursor position (as if, the
  // the user has pressed delete key in the textfield). Returns true if
  // the deletion is successful.
  // If there is composition text, it'll be deleted instead.
  bool Delete();

  // Deletes the first character before the current cursor position (as if, the
  // the user has pressed backspace key in the textfield). Returns true if
  // the removal is successful.
  // If there is composition text, it'll be deleted instead.
  bool Backspace();

  // Cursor related methods.

  // Returns the current cursor position.
  size_t cursor_pos() const { return cursor_pos_; }

  // Moves the cursor left by one position (as if, the user has pressed the left
  // arrow key). If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  void MoveCursorLeft(bool select);

  // Moves the cursor right by one position (as if, the user has pressed the
  // right arrow key). If |select| is true, it updates the selection
  // accordingly.
  // The current composition text will be confirmed.
  void MoveCursorRight(bool select);

  // Moves the cursor left by one word (word boundry is defined by space).
  // If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  void MoveCursorToPreviousWord(bool select);

  // Moves the cursor right by one word (word boundry is defined by space).
  // If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  void MoveCursorToNextWord(bool select);

  // Moves the cursor to start of the textfield contents.
  // If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  void MoveCursorToHome(bool select);

  // Moves the cursor to end of the textfield contents.
  // If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  void MoveCursorToEnd(bool select);

  // Moves the cursor to the specified |position|.
  // If |select| is true, it updates the selection accordingly.
  // The current composition text will be confirmed.
  bool MoveCursorTo(size_t position, bool select);

  // Returns the bounds of character at the current cursor.
  gfx::Rect GetCursorBounds(const gfx::Font& font) const;

  // Selection related method

  // Returns the selected text.
  string16 GetSelectedText() const;

  void GetSelectedRange(ui::Range* range) const;

  // The current composition text will be confirmed. The
  // selection starts with the range's start position,
  // and ends with the range's end position, therefore
  // the cursor position becomes the end position.
  void SelectRange(const ui::Range& range);

  // Selects all text.
  // The current composition text will be confirmed.
  void SelectAll();

  // Selects the word at which the cursor is currently positioned.
  // The current composition text will be confirmed.
  void SelectWord();

  // Clears selection.
  // The current composition text will be confirmed.
  void ClearSelection();

  // Returns true if there is an undoable edit.
  bool CanUndo();

  // Returns true if there is an redoable edit.
  bool CanRedo();

  // Undo edit. Returns true if undo changed the text.
  bool Undo();

  // Redo edit. Returns true if redo changed the text.
  bool Redo();

  // Returns visible text. If the field is password, it returns the
  // sequence of "*".
  string16 GetVisibleText() const {
    return GetVisibleText(0U, text_.length());
  }

  // Cuts the currently selected text and puts it to clipboard. Returns true
  // if text has changed after cutting.
  bool Cut();

  // Copies the currently selected text and puts it to clipboard.
  void Copy();

  // Pastes text from the clipboard at current cursor position. Returns true
  // if text has changed after pasting.
  bool Paste();

  // Tells if any text is selected, even if the selection is in composition
  // text.
  bool HasSelection() const;

  // Deletes the selected text. This method shouldn't be called with
  // composition text.
  void DeleteSelection();

  // Deletes the selected text (if any) and insert text at given
  // position.
  void DeleteSelectionAndInsertTextAt(
      const string16& text, size_t position);

  // Retrieves the text content in a given range.
  string16 GetTextFromRange(const ui::Range& range) const;

  // Retrieves the range containing all text in the model.
  void GetTextRange(ui::Range* range) const;

  // Sets composition text and attributes. If there is composition text already,
  // it’ll be replaced by the new one. Otherwise, current selection will be
  // replaced. If there is no selection, the composition text will be inserted
  // at the insertion point.
  // Any changes to the model except text insertion will confirm the current
  // composition text.
  void SetCompositionText(const ui::CompositionText& composition);

  // Converts current composition text into final content.
  void ConfirmCompositionText();

  // Removes current composition text.
  void ClearCompositionText();

  // Retrieves the range of current composition text.
  void GetCompositionTextRange(ui::Range* range) const;

  // Returns true if there is composition text.
  bool HasCompositionText() const;

 private:
  friend class NativeTextfieldViews;
  friend class NativeTextfieldViewsTest;
  friend class TextfieldViewsModelTest;
  friend class UndoRedo_BasicTest;
  friend class UndoRedo_CutCopyPasteTest;
  friend class UndoRedo_ReplaceTest;
  friend class internal::Edit;

  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, UndoRedo_BasicTest);
  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, UndoRedo_CutCopyPasteTest);
  FRIEND_TEST_ALL_PREFIXES(TextfieldViewsModelTest, UndoRedo_ReplaceTest);

  // Returns the visible text given |start| and |end|.
  string16 GetVisibleText(size_t start, size_t end) const;

  // Utility for SelectWord(). Checks whether position pos is at word boundary.
  bool IsPositionAtWordSelectionBoundary(size_t pos);

  // Returns the normalized cursor position that does not exceed the
  // text length.
  size_t GetSafePosition(size_t position) const;

  // Insert the given |text|. |mergeable| indicates if this insert
  // operation can be merged to previous edit in the edit history.
  void InsertTextInternal(const string16& text, bool mergeable);

  // Replace the current text with the given |text|. |mergeable|
  // indicates if this replace operation can be merged to previous
  // edit in the edit history.
  void ReplaceTextInternal(const string16& text, bool mergeable);

  // Clears all edit history.
  void ClearEditHistory();

  // Clears redo history.
  void ClearRedoHistory();

  // Executes and records edit operations.
  void ExecuteAndRecordDelete(size_t from, size_t to, bool mergeable);
  void ExecuteAndRecordReplace(const string16& text, bool mergeable);
  void ExecuteAndRecordReplaceAt(const string16& text,
                                 size_t at,
                                 bool mergeable);
  void ExecuteAndRecordInsert(const string16& text, bool mergeable);

  // Adds or merge |edit| into edit history. Return true if the edit
  // has been merged and must be deleted after redo.
  bool AddOrMergeEditHistory(internal::Edit* edit);

  // Modify the text buffer in following way:
  // 1) Delete the string from |delete_from| to |delte_to|.
  // 2) Insert the |new_text| at the index |new_text_insert_at|.
  //    Note that the index is after deletion.
  // 3) Move the cursor to |new_cursor_pos|.
  void ModifyText(size_t delete_from,
                  size_t delete_to,
                  const string16& new_text,
                  size_t new_text_insert_at,
                  size_t new_cursor_pos);

  // Pointer to a TextfieldViewsModel::Delegate instance, should be provided by
  // the View object.
  Delegate* delegate_;

  // The text in utf16 format.
  string16 text_;

  // Current cursor position.
  size_t cursor_pos_;

  // Selection range.
  size_t selection_start_;

  // Composition text range.
  size_t composition_start_;
  size_t composition_end_;

  // Underline information of the composition text.
  ui::CompositionUnderlines composition_underlines_;

  // True if the text is the password.
  bool is_password_;

  typedef std::list<internal::Edit*> EditHistory;
  EditHistory edit_history_;

  // An iterator that points to the current edit that can be undone.
  // This iterator moves from the |end()|, meaining no edit to undo,
  // to the last element (one before |end()|), meaning no edit to redo.
  //  There is no edit to undo (== end()) when:
  // 1) in initial state. (nothing to undo)
  // 2) very 1st edit is undone.
  // 3) all edit history is removed.
  //  There is no edit to redo (== last element or no element) when:
  // 1) in initial state. (nothing to redo)
  // 2) new edit is added. (redo history is cleared)
  // 3) redone all undone edits.
  EditHistory::iterator current_edit_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldViewsModel);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
