// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#pragma once

#include "base/string16.h"
#include "base/task.h"
#include "gfx/font.h"
#include "views/border.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#include "views/view.h"

namespace gfx {
class Canvas;
}  // namespace

namespace views {

class KeyEvent;
class TextfieldViewsModel;

// A views/skia only implementation of NativeTextfieldWrapper.
// No platform specific code is used.
// Following features are not yet supported.
// * BIDI
// * Clipboard  (Cut & Paste).
// * Context Menu.
// * IME/i18n support.
// * X selection (only if we want to support).
// * STYLE_MULTILINE, STYLE_LOWERCASE text. (These are not used in
//   chromeos, so we may not need them)
// * Double click to select word, and triple click to select all.
class NativeTextfieldViews : public views::View,
                             public NativeTextfieldWrapper {
 public:
  explicit NativeTextfieldViews(Textfield* parent);
  ~NativeTextfieldViews();

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& e);
  virtual bool OnMouseDragged(const views::MouseEvent& e);
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual bool OnKeyPressed(const views::KeyEvent& e);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual void Paint(gfx::Canvas* canvas);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual void WillGainFocus();
  virtual void DidGainFocus();
  virtual void WillLoseFocus();

  // NativeTextfieldWrapper overrides:
  virtual string16 GetText() const;
  virtual void UpdateText();
  virtual void AppendText(const string16& text);
  virtual string16 GetSelectedText() const;
  virtual void SelectAll();
  virtual void ClearSelection();
  virtual void UpdateBorder();
  virtual void UpdateTextColor();
  virtual void UpdateBackgroundColor();
  virtual void UpdateReadOnly();
  virtual void UpdateFont();
  virtual void UpdateIsPassword();
  virtual void UpdateEnabled();
  virtual gfx::Insets CalculateInsets();
  virtual void UpdateHorizontalMargins();
  virtual void UpdateVerticalMargins();
  virtual bool SetFocus();
  virtual View* GetView();
  virtual gfx::NativeView GetTestingHandle() const;
  virtual bool IsIMEComposing() const;
  virtual bool HandleKeyPressed(const views::KeyEvent& e);
  virtual bool HandleKeyReleased(const views::KeyEvent& e);
  virtual void HandleWillGainFocus();
  virtual void HandleDidGainFocus();
  virtual void HandleWillLoseFocus();

  // class name of internal
  static const char kViewClassName[];

  // Returns true when
  // 1) built with GYP_DEFIENS="touchui=1"
  // 2) enabled by SetEnableTextfieldViews(true)
  // 3) enabled by the command line flag "--enable-textfield-view".
  static bool IsTextfieldViewsEnabled();
  // Enable/Disable TextfieldViews implementation for Textfield.
  static void SetEnableTextfieldViews(bool enabled);

 private:
  friend class NativeTextfieldViewsTest;

  // A Border class to draw focus border for the text field.
  class TextfieldBorder : public Border {
   public:
    TextfieldBorder();

    // Border implementation.
    virtual void Paint(const View& view, gfx::Canvas* canvas) const;
    virtual void GetInsets(gfx::Insets* insets) const;

    // Sets the insets of the border.
    void SetInsets(int top, int left, int bottom, int right);

    // Sets the focus state.
    void set_has_focus(bool has_focus) {
      has_focus_ = has_focus;
    }

   private:
    bool has_focus_;
    gfx::Insets insets_;

    DISALLOW_COPY_AND_ASSIGN(TextfieldBorder);
  };

  // Returns the Textfield's font.
  const gfx::Font& GetFont() const;

  // Returns the Textfield's text color.
  SkColor GetTextColor() const;

  // A callback function to periodically update the cursor state.
  void UpdateCursor();

  // Repaint the cursor.
  void RepaintCursor();

  // Update the cursor_bounds and text_offset.
  void UpdateCursorBoundsAndTextOffset();

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Handle the keyevent.
  bool HandleKeyEvent(const KeyEvent& key_event);

  // Utility function. Gets the character corresponding to a keyevent.
  // Returns 0 if the character is not printable.
  char16 GetPrintableChar(const KeyEvent& key_event);

  // Find a cusor position for given |point| in this views coordinates.
  size_t FindCursorPosition(const gfx::Point& point) const;

  // The parent textfield, the owner of this object.
  Textfield* textfield_;

  // The text model.
  scoped_ptr<TextfieldViewsModel> model_;

  // The reference to the border class. The object is owned by View::border_.
  TextfieldBorder* text_border_;

  // The x offset for the text to be drawn, without insets;
  int text_offset_;

  // Cursor's bounds in the textfield's coordinates.
  gfx::Rect cursor_bounds_;

  // True if the textfield is in insert mode.
  bool insert_;

  // The drawing state of cursor. True to draw.
  bool is_cursor_visible_;

  // A runnable method factory for callback to update the cursor.
  ScopedRunnableMethodFactory<NativeTextfieldViews> cursor_timer_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViews);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
