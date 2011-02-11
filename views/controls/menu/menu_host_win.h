// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#pragma once

#include "views/controls/menu/menu_host.h"
#include "views/widget/native_widget_win.h"

namespace views {

class SubmenuView;

// MenuHost implementation for windows.
class MenuHostWin : public NativeWidgetWin, public MenuHost {
 public:
  explicit MenuHostWin(SubmenuView* submenu);
  virtual ~MenuHostWin();

  // MenuHost overrides:
  virtual void Init(HWND parent,
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

  // NativeWidgetWin overrides:
  virtual void OnDestroy();
  virtual void OnCaptureChanged(HWND hwnd);
  virtual void OnCancelMode();

 protected:
  virtual RootView* CreateRootView();

  // Overriden to return false, we do NOT want to release capture on mouse
  // release.
  virtual bool ReleaseCaptureOnMouseReleased();

 private:
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
