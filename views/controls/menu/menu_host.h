// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

namespace views {

class SubmenuView;
class View;

// SubmenuView uses a MenuHost to house the SubmenuView. MenuHost typically
// extends the native Widget type, but is defined here for clarity of what
// methods SubmenuView uses.
//
// SubmenuView owns the MenuHost. When SubmenuView is done with the MenuHost
// |DestroyMenuHost| is invoked. The one exception to this is if the native
// OS destroys the widget out from under us, in which case |MenuHostDestroyed|
// is invoked back on the SubmenuView and the SubmenuView then drops references
// to the MenuHost.
class MenuHost {
 public:
  // Creates the platform specific MenuHost. Ownership passes to the caller.
  static MenuHost* Create(SubmenuView* submenu_view);

  // Initializes and shows the MenuHost.
  virtual void InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) = 0;

  // Returns true if the menu host is visible.
  virtual bool IsMenuHostVisible() = 0;

  // Shows the menu host. If |do_capture| is true the menu host should do a
  // mouse grab.
  virtual void ShowMenuHost(bool do_capture) = 0;

  // Hides the menu host.
  virtual void HideMenuHost() = 0;

  // Destroys and deletes the menu host.
  virtual void DestroyMenuHost() = 0;

  // Sets the bounds of the menu host.
  virtual void SetMenuHostBounds(const gfx::Rect& bounds) = 0;

  // Releases a mouse grab installed by |ShowMenuHost|.
  virtual void ReleaseMenuHostCapture() = 0;

  // Returns the native window of the MenuHost.
  virtual gfx::NativeWindow GetMenuHostWindow() = 0;

 protected:
  virtual ~MenuHost() {}
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_H_
