// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#define VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
#pragma once

#include "views/controls/button/checkbox.h"

namespace views {

// A native themed class representing a radio button.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
class VIEWS_API RadioButton : public Checkbox {
 public:
  // The button's class name.
  static const char kViewClassName[];

  RadioButton(const std::wstring& label, int group_id);
  virtual ~RadioButton();

  // Overridden from View:
  virtual std::string GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual View* GetSelectedViewForGroup(int group) OVERRIDE;
  virtual bool IsGroupFocusTraversable() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  // Overridden from Button:
  virtual void NotifyClick(const views::Event& event) OVERRIDE;

  // Overridden from TextButtonBase:
  virtual gfx::NativeTheme::Part GetThemePart() const OVERRIDE;

  // Overridden from Checkbox:
  virtual void SetChecked(bool checked) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(RadioButton);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_RADIO_BUTTON_H_
