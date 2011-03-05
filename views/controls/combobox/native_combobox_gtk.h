// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
#define VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
#pragma once

#include "ui/base/gtk/gtk_signal.h"
#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace views {

class NativeComboboxGtk : public NativeControlGtk,
                          public NativeComboboxWrapper {
 public:
  explicit NativeComboboxGtk(Combobox* combobox);
  virtual ~NativeComboboxGtk();

  // Overridden from NativeComboboxWrapper:
  virtual void UpdateFromModel();
  virtual void UpdateSelectedItem();
  virtual void UpdateEnabled();
  virtual int GetSelectedItem() const;
  virtual bool IsDropdownOpen() const;
  virtual gfx::Size GetPreferredSize();
  virtual View* GetView();
  virtual void SetFocus();
  virtual gfx::NativeView GetTestingHandle() const;

 protected:
  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

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

  // The preferred size from the last size_request. See
  // NativeButtonGtk::preferred_size_ for more detail why we need this.
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeComboboxGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_GTK_H_
