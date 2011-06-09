// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "views/widget/native_widget_gtk.h"
#include "views/window/native_window.h"

namespace views {
namespace internal {
class NativeWindowDelegate;
}

// Window implementation for Gtk.
class NativeWindowGtk : public NativeWidgetGtk, public NativeWindow {
 public:
  explicit NativeWindowGtk(internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowGtk();

  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;

 protected:
  // Overridden from NativeWindow:
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;

  // For  the constructor.
  friend class Window;

 private:
  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowGtk);
};

}  // namespace views

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
