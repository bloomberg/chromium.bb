// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect.h"

namespace gfx {
class Font;
}  // namespace gfx

namespace views {

class TextRange;

// A model that represents a text content for TextfieldViews.
// It supports editing, selection and cursor manipulation.
class TextfieldViewsModel {
 public:
  TextfieldViewsModel();
  virtual ~TextfieldViewsModel();

  // Text fragment info. Used to draw selected text.
  // We may replace this with TextAttribute class
  // in the future to support multi-color text
  // for omnibox.
  struct TextFragment {
    TextFragment(size_t b, size_t e, bool s)
        : begin(b), end(e), selected(s) {
    }
    // The begin and end position of text fragment.
    size_t begin, end;
    // True if the text is selected.
    bool selected;
  };
  typedef std::vector<TextFragment> TextFragments;

  // Gets the text element info.
  void GetFragments(TextFragments* elements) const;

  void set_is_password(bool is_password) {
    is_password_ = is_password;
  }
  const string16& text() const { return text_; }

  // Edit related methods.

  // Sest the text. Returns true if the text has been modified.
  bool SetText(const string16& text);

  // Inserts a character at the current cursor position.
  void Insert(char16 c);

  // Replaces the char at the current position with given character.
  void Replace(char16 c);

  // Appends the text.
  void Append(const string16& text);

  // Deletes the first character after the current cursor position (as if, the
  // the user has pressed delete key in the textfield). Returns true if
  // the deletion is successful.
  bool Delete();

  // Deletes the first character before the current cursor position (as if, the
  // the user has pressed backspace key in the textfield). Returns true if
  // the removal is successful.
  bool Backspace();

  // Cursor related methods.

  // Returns the current cursor position.
  size_t cursor_pos() const { return cursor_pos_; }

  // Moves the cursor left by one position (as if, the user has pressed the left
  // arrow key). If |select| is true, it updates the selection accordingly.
  void MoveCursorLeft(bool select);

  // Moves the cursor right by one position (as if, the user has pressed the
  // right arrow key). If |select| is true, it updates the selection
  // accordingly.
  void MoveCursorRight(bool select);

  // Moves the cursor left by one word (word boundry is defined by space).
  // If |select| is true, it updates the selection accordingly.
  void MoveCursorToPreviousWord(bool select);

  // Moves the cursor right by one word (word boundry is defined by space).
  // If |select| is true, it updates the selection accordingly.
  void MoveCursorToNextWord(bool select);

  // Moves the cursor to start of the textfield contents.
  // If |select| is true, it updates the selection accordingly.
  void MoveCursorToStart(bool select);

  // Moves the cursor to end of the textfield contents.
  // If |select| is true, it updates the selection accordingly.
  void MoveCursorToEnd(bool select);

  // Moves the cursor to the specified |position|.
  // If |select| is true, it updates the selection accordingly.
  bool MoveCursorTo(size_t position, bool select);

  // Returns the bounds of character at the current cursor.
  gfx::Rect GetCursorBounds(const gfx::Font& font) const;

  // Selection related method

  // Returns the selected text.
  string16 GetSelectedText() const;

  void GetSelectedRange(TextRange* range) const;

  void SelectRange(const TextRange& range);

  // Selects all text.
  void SelectAll();

  // Selects the word at which the cursor is currently positioned.
  void SelectWord();

  // Clears selection.
  void ClearSelection();

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

  // Tells if any text is selected.
  bool HasSelection() const;

 private:
  friend class NativeTextfieldViews;

  // Deletes the selected text.
  void DeleteSelection();

  // Returns the visible text given |start| and |end|.
  string16 GetVisibleText(size_t start, size_t end) const;

  // Utility for SelectWord(). Checks whether position pos is at word boundary.
  bool IsPositionAtWordSelectionBoundary(size_t pos);

  // Returns the normalized cursor position that does not exceed the
  // text length.
  size_t GetSafePosition(size_t position) const;

  // The text in utf16 format.
  string16 text_;

  // Current cursor position.
  size_t cursor_pos_;

  // Selection range.
  size_t selection_begin_;

  // True if the text is the password.
  bool is_password_;

  DISALLOW_COPY_AND_ASSIGN(TextfieldViewsModel);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
