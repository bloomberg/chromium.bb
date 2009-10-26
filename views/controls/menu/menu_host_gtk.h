// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_

#include "views/widget/widget_gtk.h"

namespace views {

class SubmenuView;

// MenuHost implementation for Gtk.
class MenuHost : public WidgetGtk {
 public:
  explicit MenuHost(SubmenuView* submenu);

  void Init(gfx::NativeWindow parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool do_capture);

  gfx::NativeWindow GetNativeWindow();

  void Show();
  virtual void Hide();
  virtual void HideWindow();
  void DoCapture();
  void ReleaseCapture();

 protected:
  virtual RootView* CreateRootView();

  virtual gboolean OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased();

  // Overriden to also release pointer grab.
  virtual void ReleaseGrab();

 private:
  // If true, we've been closed.
  bool closed_;

  // The view we contain.
  SubmenuView* submenu_;

  // Have we done a pointer grab?
  bool did_pointer_grab_;

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
