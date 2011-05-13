// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
#pragma once

#include "views/controls/menu/native_menu_host.h"
#include "views/widget/widget_win.h"

namespace views {
namespace internal {
class NativeMenuHostDelegate;
}

// MenuHost implementation for windows.
class MenuHostWin : public WidgetWin,
                    public NativeMenuHost {
 public:
  explicit MenuHostWin(internal::NativeMenuHostDelegate* delegate);
  virtual ~MenuHostWin();

 private:
  // Overridden from NativeMenuHost:
  virtual void StartCapturing() OVERRIDE;
  virtual NativeWidget* AsNativeWidget() OVERRIDE;

  // Overridden from WidgetWin:
  virtual void OnDestroy() OVERRIDE;
  virtual void OnCancelMode() OVERRIDE;

  internal::NativeMenuHostDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostWin);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_WIN_H_
