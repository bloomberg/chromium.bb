// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_H_
#define VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_H_

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace views {

class SubmenuView;
class View;

class NativeMenuHost {
 public:
  virtual ~NativeMenuHost() {}

  static NativeMenuHost* CreateNativeMenuHost(SubmenuView* submenu);

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
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_H_
