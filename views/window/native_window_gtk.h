// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "views/widget/native_widget_gtk.h"
#include "views/window/native_window.h"
#include "views/window/window.h"

namespace gfx {
class Point;
class Size;
};

namespace views {
namespace internal {
class NativeWindowDelegate;
}

class Client;
class WindowDelegate;

// Window implementation for Gtk.
class NativeWindowGtk : public NativeWidgetGtk, public NativeWindow {
 public:
  explicit NativeWindowGtk(internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowGtk();

  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;

  // Overridden from NativeWidgetGtk:
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnConfigureEvent(GtkWidget* widget,
                                    GdkEventConfigure* event);
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);

 protected:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;

  // Overridden from NativeWindow:
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual void BecomeModal() OVERRIDE;

  // Overridden from NativeWidgetGtk:
  virtual gboolean OnWindowStateEvent(GtkWidget* widget,
                                      GdkEventWindowState* event) OVERRIDE;

  // For  the constructor.
  friend class Window;

 private:
  static gboolean CallConfigureEvent(GtkWidget* widget,
                                     GdkEventConfigure* event,
                                     NativeWindowGtk* window_gtk);

  // Asks the delegate if any to save the window's location and size.
  void SaveWindowPosition();

  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  // Our window delegate.
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  // Set to true if the window is in the process of closing.
  bool window_closed_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowGtk);
};

}  // namespace views

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_GTK_H_
