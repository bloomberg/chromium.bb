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

  void Init(gfx::NativeView parent,
            const gfx::Rect& bounds,
            View* contents_view,
            bool do_capture);

  void Show();
  virtual void Hide();
  virtual void HideWindow();
  void ReleaseCapture();

 protected:
  virtual RootView* CreateRootView();

  virtual void OnCancelMode();

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased();

 private:
  // If true, we've been closed.
  bool closed_;

  // The view we contain.
  SubmenuView* submenu_;

  DISALLOW_COPY_AND_ASSIGN(MenuHost);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
