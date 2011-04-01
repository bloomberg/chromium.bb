// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
#pragma once

#include <vector>

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

  // Sest the text. Returns true if the text has been modified.
  // The current composition text will be confirmed first.
  bool SetText(const string16& text);

  // Inserts given |text| at the current cursor position.
  // The current composition text will be cleared.
  void InsertText(const string16& text);

  // Inserts a character at the current cursor position.
  void InsertChar(char16 c) {
    InsertText(string16(&c, 1));
  }

  // Replaces characters at the current position with characters in given text.
  // The current composition text will be cleared.
  void ReplaceText(const string16& text);

  // Replaces the char at the current position with given character.
  void ReplaceChar(char16 c) {
    ReplaceText(string16(&c, 1));
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

  // The current composition text will be confirmed.
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

  // Returns the visible text given |start| and |end|.
  string16 GetVisibleText(size_t start, size_t end) const;

  // Utility for SelectWord(). Checks whether position pos is at word boundary.
  bool IsPositionAtWordSelectionBoundary(size_t pos);

  // Returns the normalized cursor position that does not exceed the
  // text length.
  size_t GetSafePosition(size_t position) const;

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

  DISALLOW_COPY_AND_ASSIGN(TextfieldViewsModel);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_VIEWS_MODEL_H_
