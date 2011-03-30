// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
#pragma once

// TODO(avi): remove when not needed
#include "base/utf_string_conversions.h"
#include "ui/gfx/font.h"
#include "views/controls/button/button.h"
#include "views/controls/button/native_button_wrapper.h"

namespace gfx {
class Font;
}

namespace views {

class NativeButton : public Button {
 public:
  // The button's class name.
  static const char kViewClassName[];

  explicit NativeButton(ButtonListener* listener);
  NativeButton(ButtonListener* listener, const std::wstring& label);
  virtual ~NativeButton();

  // Sets/Gets the text to be used as the button's label.
  virtual void SetLabel(const std::wstring& label);
  std::wstring label() const { return UTF16ToWideHack(label_); }

  // Sets the font to be used when displaying the button's label.
  void set_font(const gfx::Font& font) { font_ = font; }
  const gfx::Font& font() const { return font_; }

  // Sets/Gets whether or not the button appears and behaves as the default
  // button in its current context.
  void SetIsDefault(bool default_button);
  bool is_default() const { return is_default_; }

  // Sets/Gets whether or not pressing this button requires elevation.
  void SetNeedElevation(bool need_elevation);
  bool need_elevation() const { return need_elevation_; }

  void set_ignore_minimum_size(bool ignore_minimum_size) {
    ignore_minimum_size_ = ignore_minimum_size;
  }

  // Called by the wrapper when the actual wrapped native button was pressed.
  void ButtonPressed();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void SetEnabled(bool flag) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 protected:
  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool AcceleratorPressed(const Accelerator& accelerator) OVERRIDE;

  // Create the button wrapper and returns it. Ownership of the returned
  // value is passed to the caller.
  //
  // This can be overridden by subclass to create a wrapper of a particular
  // type. See NativeButtonWrapper interface for types.
  virtual NativeButtonWrapper* CreateWrapper();

  // Sets a border to the button. Override to set a different border or to not
  // set one (the default is 0,8,0,8 for push buttons).
  virtual void InitBorder();

  // The object that actually implements the native button.
  NativeButtonWrapper* native_wrapper_;

 private:
  // The button label.
  string16 label_;

  // True if the button is the default button in its context.
  bool is_default_;

  // True if this button requires elevation (or sudo). This flag is currently
  // used for adding a shield icon on Windows Vista or later.
  bool need_elevation_;

  // The font used to render the button label.
  gfx::Font font_;

  // True if the button should ignore the minimum size for the platform. Default
  // is false. Set to true to create narrower buttons.
  bool ignore_minimum_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_H_
