// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
#pragma once

#include "views/controls/menu/menu_host.h"
#include "views/widget/widget_gtk.h"

namespace views {

class SubmenuView;

// MenuHost implementation for Gtk.
class MenuHostGtk : public WidgetGtk, public MenuHost {
 public:
  explicit MenuHostGtk(SubmenuView* submenu);
  virtual ~MenuHostGtk();

  // MenuHost overrides.
  virtual void InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture);
  virtual bool IsMenuHostVisible();
  virtual void ShowMenuHost(bool do_capture);
  virtual void HideMenuHost();
  virtual void DestroyMenuHost();
  virtual void SetMenuHostBounds(const gfx::Rect& bounds);
  virtual void ReleaseMenuHostCapture();
  virtual gfx::NativeWindow GetMenuHostWindow();

 protected:
  virtual RootView* CreateRootView();

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased();

  // Overriden to also release input grab.
  virtual void ReleaseGrab();

  virtual void OnDestroy(GtkWidget* object);
  virtual gboolean OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);

 private:
  void DoCapture();

  // If true, DestroyMenuHost has been invoked.
  bool destroying_;

  // The view we contain.
  SubmenuView* submenu_;

  // Have we done input grab?
  bool did_input_grab_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_GTK_H_
