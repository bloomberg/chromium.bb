// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_MESSAGE_FILTER_H_
#define UI_VIEWS_WIDGET_WIDGET_MESSAGE_FILTER_H_
#pragma once

#include "ui/base/win/hwnd_subclass.h"

namespace aura {
class RootWindow;
}

namespace gfx {
class Insets;
}

namespace views {

class Widget;

// A message filter that implements a custom frame look for views Widgets.
class WidgetMessageFilter : public ui::HWNDMessageFilter {
 public:
  WidgetMessageFilter(aura::RootWindow* root_window, Widget* widget);
  virtual ~WidgetMessageFilter();

 private:
  // Overridden from ui::HWNDMessageFilter:
  virtual bool FilterMessage(HWND hwnd,
                             UINT message,
                             WPARAM w_param,
                             LPARAM l_param,
                             LRESULT* l_result) OVERRIDE;

  // Message handlers.
  bool OnNCCalcSize(WPARAM w_param, LPARAM l_param, LRESULT* l_result);
  bool OnNCHitTest(int x, int y, LRESULT* l_result);
  bool OnNCPaint(HRGN update_region);
  void OnSize();
  void OnWindowPosChanged(WINDOWPOS* window_pos);

  // TODO(beng): Lift documentation from NativeWidgetWin.
  gfx::Insets GetClientAreaInsets() const;
  bool WidgetSizeIsClientSize() const;
  void ClientAreaSizeChanged();
  void ResetWindowRegion(bool force);

  aura::RootWindow* root_window_;
  Widget* widget_;
  HWND hwnd_;

  DISALLOW_COPY_AND_ASSIGN(WidgetMessageFilter);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_MESSAGE_FILTER_H_
