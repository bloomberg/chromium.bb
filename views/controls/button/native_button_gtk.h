// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
#define VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_

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
  virtual gfx::NativeView GetTestingHandle() const;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();

 protected:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

  // Invoked when the user clicks on the button.
  virtual void OnClicked();

  // Returns true if this button is actually a checkbox or radio button.
  virtual bool IsCheckbox() const { return false; }

 private:
  static void CallClicked(GtkButton* widget, NativeButtonGtk* button);

  // The NativeButton we are bound to.
  NativeButton* native_button_;

  // The preferred size from the last size_request. We save this until we are
  // notified that data may have caused the preferred size to change because
  // otherwise it seems every time we call gtk_widget_size_request the size
  // returned is a little larger (?!).
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(NativeButtonGtk);
};

class NativeCheckboxGtk : public NativeButtonGtk {
 public:
  explicit NativeCheckboxGtk(Checkbox* checkbox);

 private:
  static void CallClicked(GtkButton* widget, NativeCheckboxGtk* button);

  virtual void CreateNativeControl();

  // Invoked when the user clicks on the button.
  virtual void OnClicked();

  // Overidden from NativeButtonWrapper
  virtual void UpdateChecked();

  // Returns true if this button is actually a checkbox or radio button.
  virtual bool IsCheckbox() const { return true; }

  Checkbox* checkbox_;

  // a flag to prevent OnClicked event when updating
  // gtk control via API gtk_toggle_button_set_active.
  bool deliver_click_event_;

  DISALLOW_COPY_AND_ASSIGN(NativeCheckboxGtk);
};

}  // namespace views

#endif  // #ifndef VIEWS_CONTROLS_BUTTON_NATIVE_BUTTON_GTK_H_
