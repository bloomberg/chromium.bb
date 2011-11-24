// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
#pragma once

#include "ui/base/gtk/gtk_signal.h"
#include "ui/views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace views {

class NativeComboboxGtk : public NativeControlGtk,
                          public NativeComboboxWrapper {
 public:
  explicit NativeComboboxGtk(Combobox* combobox);
  virtual ~NativeComboboxGtk();

  // Overridden from NativeComboboxWrapper:
  virtual void UpdateFromModel() OVERRIDE;
  virtual void UpdateSelectedItem() OVERRIDE;
  virtual void UpdateEnabled() OVERRIDE;
  virtual int GetSelectedItem() const OVERRIDE;
  virtual bool IsDropdownOpen() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual bool HandleKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool HandleKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;

 protected:
  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl() OVERRIDE;
  virtual void NativeControlCreated(GtkWidget* widget) OVERRIDE;

 private:
  void SelectionChanged();
  void FocusedMenuItemChanged();

  CHROMEGTK_CALLBACK_0(NativeComboboxGtk, void, CallChanged);
  CHROMEGTK_CALLBACK_0(NativeComboboxGtk, gboolean, CallPopUp);
  CHROMEGTK_CALLBACK_1(NativeComboboxGtk, void, CallMenuMoveCurrent,
                       GtkMenuDirectionType);

  // The combobox we are bound to.
  Combobox* combobox_;

  // The combo box's pop-up menu.
  GtkMenu* menu_;

  // The preferred size from the last size_request.
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeComboboxGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
