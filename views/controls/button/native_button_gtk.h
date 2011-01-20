// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
#define VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
#pragma once

#include "ui/base/gtk/gtk_signal.h"
#include "views/controls/button/native_button_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace views {

// A View that hosts a native GTK button.
class NativeButtonGtk : public NativeControlGtk, public NativeButtonWrapper {
 public:
  explicit NativeButtonGtk(NativeButton* native_button);
  virtual ~NativeButtonGtk();

  // Overridden from NativeButtonWrapper:
  virtual void UpdateLabel();
  virtual void UpdateFont();
  virtual void UpdateEnabled();
  virtual void UpdateDefault();
  virtual View* GetView();
  virtual void SetFocus();
  virtual bool UsesNativeLabel() const;
  virtual bool UsesNativeRadioButtonGroup() const;
  virtual gfx::NativeView GetTestingHandle() const;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

 protected:
  CHROMEG_CALLBACK_0(NativeButtonGtk, void, CallClicked, GtkButton*);

  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

  // Invoked when the user clicks on the button.
  virtual void OnClicked();

  // The NativeButton we are bound to.
  NativeButton* native_button_;

 private:
  // The preferred size from the last size_request. We save this until we are
  // notified that data may have caused the preferred size to change because
  // otherwise it seems every time we call gtk_widget_size_request the size
  // returned is a little larger (?!).
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonGtk);
};

// A View that hosts a native Gtk checkbox button.
class NativeCheckboxGtk : public NativeButtonGtk {
 public:
  explicit NativeCheckboxGtk(Checkbox* checkbox);

 protected:
  // Update checkbox's check state.
  void SyncCheckState();

 private:
  // Return Checkbox we are bound to.
  Checkbox* checkbox();

  virtual void CreateNativeControl();

  // Invoked when the user clicks on the button.
  virtual void OnClicked();

  // Overidden from NativeButtonWrapper
  virtual void UpdateChecked();
  virtual void UpdateDefault();

  // A flag to prevent OnClicked event when updating
  // gtk control via API gtk_toggle_button_set_active.
  bool deliver_click_event_;

  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxGtk);
};

// A View that hosts a native Gtk radio button.
class NativeRadioButtonGtk : public NativeCheckboxGtk {
 public:
  explicit NativeRadioButtonGtk(RadioButton* radio_button);
  virtual ~NativeRadioButtonGtk();

 protected:
  // Overridden from NativeCheckboxGtk.
  virtual void CreateNativeControl();

  // Overridden from NativeControlGtk.
  virtual void ViewHierarchyChanged(bool is_add, View *parent, View *child);

 private:
  static void CallToggled(GtkButton* widget, NativeRadioButtonGtk* button);

  // Return RadioButton we are bound to.
  RadioButton* radio_button();
  // Set the gtk radio button's group to that of given wrapper's gruop.
  void SetGroupFrom(NativeButtonWrapper* wrapper);
  // Invoked when the radio button's state is changed.
  void OnToggled();

  DISALLOW_COPY_AND_ASSIGN(NativeRadioButtonGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
