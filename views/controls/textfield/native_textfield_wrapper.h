// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_
#pragma once

#include "base/string16.h"
#include "gfx/native_widget_types.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {

class Textfield;
class View;

// An interface implemented by an object that provides a platform-native
// text field.
class NativeTextfieldWrapper {
 public:
  // The Textfield calls this when it is destroyed to clean up the wrapper
  // object.
  virtual ~NativeTextfieldWrapper() {}

  // Gets the text displayed in the wrapped native text field.
  virtual string16 GetText() const = 0;

  // Updates the text displayed with the text held by the Textfield.
  virtual void UpdateText() = 0;

  // Adds the specified text to the text already displayed by the wrapped native
  // text field.
  virtual void AppendText(const string16& text) = 0;

  // Gets the text that is selected in the wrapped native text field.
  virtual string16 GetSelectedText() const = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll() = 0;

  // Clears the selection within the edit field and sets the caret to the end.
  virtual void ClearSelection() = 0;

  // Updates the border display for the native text field with the state desired
  // by the Textfield.
  virtual void UpdateBorder() = 0;

  // Updates the text color used when painting the native text field.
  virtual void UpdateTextColor() = 0;

  // Updates the background color used when painting the native text field.
  virtual void UpdateBackgroundColor() = 0;

  // Updates the read-only state of the native text field.
  virtual void UpdateReadOnly() = 0;

  // Updates the font used to render text in the native text field.
  virtual void UpdateFont() = 0;

  // Updates the visibility of the text in the native text field.
  virtual void UpdateIsPassword() = 0;

  // Updates the enabled state of the native text field.
  virtual void UpdateEnabled() = 0;

  // Returns the insets for the text field.
  virtual gfx::Insets CalculateInsets() = 0;

  // Updates the horizontal margins for the native text field.
  virtual void UpdateHorizontalMargins() = 0;

  // Updates the vertical margins for the native text field.
  virtual void UpdateVerticalMargins() = 0;

  // Sets the focus to the text field.
  virtual void SetFocus() = 0;

  // Retrieves the views::View that hosts the native control.
  virtual View* GetView() = 0;

  // Returns a handle to the underlying native view for testing.
  virtual gfx::NativeView GetTestingHandle() const = 0;

  // Returns whether or not an IME is composing text.
  virtual bool IsIMEComposing() const = 0;

  // Creates an appropriate NativeTextfieldWrapper for the platform.
  static NativeTextfieldWrapper* CreateWrapper(Textfield* field);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_WRAPPER_H_
