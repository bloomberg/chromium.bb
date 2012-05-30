// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_message_filter.h"

#include <windowsx.h>

#include "ui/aura/root_window.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/window/non_client_view.h"

namespace views {

WidgetMessageFilter::WidgetMessageFilter(aura::RootWindow* root_window,
                                         Widget* widget)
    : root_window_(root_window),
      widget_(widget) {
  hwnd_ = root_window_->GetAcceleratedWidget();
  ClientAreaSizeChanged();
}

WidgetMessageFilter::~WidgetMessageFilter() {
}

bool WidgetMessageFilter::FilterMessage(HWND hwnd,
                                        UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param,
                                        LRESULT* l_result) {
  switch (message) {
    case WM_NCCALCSIZE:
      return OnNCCalcSize(w_param, l_param, l_result);
    case WM_NCHITTEST:
      return OnNCHitTest(GET_X_LPARAM(l_param),
                         GET_Y_LPARAM(l_param),
                         l_result);
    case WM_NCPAINT:
      return OnNCPaint(reinterpret_cast<HRGN>(w_param));
    case WM_SIZE:
      OnSize();
      return false;
    case WM_WINDOWPOSCHANGED:
      OnWindowPosChanged(reinterpret_cast<WINDOWPOS*>(l_param));
      return false;
  }
  return false;
}

bool WidgetMessageFilter::OnNCCalcSize(WPARAM w_param,
                                       LPARAM l_param,
                                       LRESULT* l_result) {
  gfx::Insets insets = GetClientAreaInsets();
  if (insets.empty()) {
    *l_result = 0;
    return false;
  }

  RECT* client_rect = w_param ?
      &(reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param)->rgrc[0]) :
      reinterpret_cast<RECT*>(l_param);
  client_rect->left += insets.left();
  client_rect->top += insets.top();
  client_rect->bottom -= insets.bottom();
  client_rect->right -= insets.right();

  if (insets.left() == 0 || insets.top() == 0)
    *l_result = 0;
  else
    *l_result = w_param ? WVR_REDRAW : 0;
  return true;
}

bool WidgetMessageFilter::OnNCHitTest(int x, int y, LRESULT* l_result) {
  POINT temp = { x, y };
  MapWindowPoints(HWND_DESKTOP, hwnd_, &temp, 1);
  int component = widget_->GetNonClientComponent(gfx::Point(temp));
  if (component != HTNOWHERE) {
    *l_result = component;
    return true;
  }
  return false;
}

bool WidgetMessageFilter::OnNCPaint(HRGN update_region) {
  NOTIMPLEMENTED();
  return true;
}

void WidgetMessageFilter::OnSize() {
  // ResetWindowRegion is going to trigger WM_NCPAINT. By doing it after we've
  // invoked OnSize we ensure the RootView has been laid out.
  ResetWindowRegion(false);
}

void WidgetMessageFilter::OnWindowPosChanged(WINDOWPOS* window_pos) {
  if (DidClientAreaSizeChange(window_pos))
    ClientAreaSizeChanged();
}

gfx::Insets WidgetMessageFilter::GetClientAreaInsets() const {
  // TODO(beng): native frame
  return gfx::Insets(0, 0, 1, 0);
}

bool WidgetMessageFilter::WidgetSizeIsClientSize() const {
  // TODO(beng):
  return false;
}

void WidgetMessageFilter::ClientAreaSizeChanged() {
  RECT r;
  if (WidgetSizeIsClientSize())
    GetClientRect(hwnd_, &r);
  else
    GetWindowRect(hwnd_, &r);
  gfx::Size s(std::max(0, static_cast<int>(r.right - r.left)),
              std::max(0, static_cast<int>(r.bottom - r.top)));
  widget_->OnNativeWidgetSizeChanged(s);
}

void WidgetMessageFilter::ResetWindowRegion(bool force) {
  NOTIMPLEMENTED();
}


}  // namespace views
