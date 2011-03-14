// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#pragma once

#include "views/controls/button/checkbox.h"

namespace views {

class NativeRadioButtonGtk;

// A Checkbox subclass representing a radio button.
class RadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked) OVERRIDE;

  // Overridden from View:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual View* GetSelectedViewForGroup(int group_id) OVERRIDE;
  virtual bool IsGroupFocusTraversable() const OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled)
      OVERRIDE;

 protected:
  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from NativeButton:
  virtual NativeButtonWrapper* CreateWrapper();

 private:
  friend class NativeRadioButtonGtk;

  NativeButtonWrapper* native_wrapper() { return native_wrapper_; }

  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
