// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#pragma once

#include "views/controls/menu/native_menu_host.h"
#include "views/widget/widget_win.h"

namespace views {

class SubmenuView;

// MenuHost implementation for windows.
class MenuHostWin : public WidgetWin,
                    public NativeMenuHost {
 public:
  explicit MenuHostWin(SubmenuView* submenu);
  virtual ~MenuHostWin();

 private:
  // Overridden from NativeMenuHost:
  virtual void InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) OVERRIDE;
  virtual bool IsMenuHostVisible() OVERRIDE;
  virtual void ShowMenuHost(bool do_capture) OVERRIDE;
  virtual void HideMenuHost() OVERRIDE;
  virtual void DestroyMenuHost() OVERRIDE;
  virtual void SetMenuHostBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void ReleaseMenuHostCapture() OVERRIDE;
  virtual gfx::NativeWindow GetMenuHostWindow() OVERRIDE;

  // Overridden from WidgetWin:
  virtual void OnDestroy() OVERRIDE;
  virtual void OnCaptureChanged(HWND hwnd) OVERRIDE;
  virtual void OnCancelMode() OVERRIDE;
  virtual RootView* CreateRootView() OVERRIDE;
  virtual bool ShouldReleaseCaptureOnMouseReleased() const OVERRIDE;

  void DoCapture();

  // If true, DestroyMenuHost has been invoked.
  bool destroying_;

  // If true, we own the capture and need to release it.
  bool owns_capture_;

  // The view we contain.
  SubmenuView* submenu_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
