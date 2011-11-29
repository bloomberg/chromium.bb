// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_CONTROL_GTK_H_
#define UI_VIEWS_CONTROLS_NATIVE_CONTROL_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "ui/views/controls/native/native_view_host.h"

namespace views {

// A View that hosts a native control.
class NativeControlGtk : public NativeViewHost {
 public:
  NativeControlGtk();
  virtual ~NativeControlGtk();

  // Overridden from View:
  virtual void OnEnabledChanged() OVERRIDE;

 protected:
  virtual void ViewHierarchyChanged(bool is_add,
                                    View *parent,
                                    View *child) OVERRIDE;
  virtual void VisibilityChanged(View* starting_from, bool is_visible) OVERRIDE;
  virtual void OnFocus() OVERRIDE;

  // Called when the NativeControlGtk is attached to a View hierarchy with a
  // valid Widget. The NativeControlGtk should use this opportunity to create
  // its associated GtkWidget.
  virtual void CreateNativeControl() = 0;

  // MUST be called by the subclass implementation of |CreateNativeControl|
  // immediately after creating the control GtkWidget, otherwise it won't be
  // attached to the GtkView and will be effectively orphaned.
  virtual void NativeControlCreated(GtkWidget* widget);

 private:
  static gboolean CallFocusIn(GtkWidget* gtk_widget,
                              GdkEventFocus* event,
                              NativeControlGtk* button);

  DISALLOW_COPY_AND_ASSIGN(NativeControlGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_CONTROL_GTK_H_
