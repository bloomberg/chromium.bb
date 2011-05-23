// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#pragma once

#include "views/controls/button/checkbox.h"

namespace views {

class NativeRadioButtonGtk;

// A Checkbox subclass representing a radio button.
class NativeRadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  NativeRadioButton(const std::wstring& label, int group_id);
  virtual ~NativeRadioButton();

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked) OVERRIDE;

  // Overridden from View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual View* GetSelectedViewForGroup(int group_id) OVERRIDE;
  virtual bool IsGroupFocusTraversable() const OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

 protected:
  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from NativeButton:
  virtual NativeButtonWrapper* CreateWrapper() OVERRIDE;

 private:
  friend class NativeRadioButtonGtk;

  NativeButtonWrapper* native_wrapper() { return native_wrapper_; }

  DISALLOW_COPY_AND_ASSIGN(NativeRadioButton);
};

// A native themed class representing a radio button.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class RadioButton : public CheckboxNt {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual View* GetSelectedViewForGroup(int group_id) OVERRIDE;
  virtual bool IsGroupFocusTraversable() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  // Overridden from Button:
  virtual void NotifyClick(const views::Event& event) OVERRIDE;

  // Overridden from TextButtonBase:
  virtual gfx::NativeTheme::Part GetThemePart() const OVERRIDE;

  // Overridden from CheckboxNt:
  virtual void SetChecked(bool checked) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
