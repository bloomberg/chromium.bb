// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/fullscreen_handler.h"

#include "base/logging.h"
// TODO(beng): Temporary dependancy until fullscreen moves to
//             HWNDMessageHandler.
#include "ui/views/widget/widget.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, public:

FullscreenHandler::FullscreenHandler(Widget* widget)
    : widget_(widget),
      fullscreen_(false),
      metro_snap_(false) {
}

FullscreenHandler::~FullscreenHandler() {
}

void FullscreenHandler::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;

  SetFullscreenImpl(fullscreen, false);
}

void FullscreenHandler::SetMetroSnap(bool metro_snap) {
  if (metro_snap_ == metro_snap)
    return;

  SetFullscreenImpl(metro_snap, true);
  metro_snap_ = metro_snap;
}

gfx::Rect FullscreenHandler::GetRestoreBounds() const {
  return gfx::Rect(saved_window_info_.window_rect);
}

////////////////////////////////////////////////////////////////////////////////
// FullscreenHandler, private:

void FullscreenHandler::SetFullscreenImpl(bool fullscreen, bool for_metro) {
  ScopedFullscreenVisibility visibility(widget_->GetNativeView());

  // Save current window state if not already fullscreen.
  if (!fullscreen_) {
    // Save current window information.  We force the window into restored mode
    // before going fullscreen because Windows doesn't seem to hide the
    // taskbar if the window is in the maximized state.
    saved_window_info_.maximized = widget_->IsMaximized();
    if (saved_window_info_.maximized)
      widget_->Restore();
    saved_window_info_.style =
        GetWindowLong(widget_->GetNativeView(), GWL_STYLE);
    saved_window_info_.ex_style =
        GetWindowLong(widget_->GetNativeView(), GWL_EXSTYLE);
    GetWindowRect(widget_->GetNativeView(), &saved_window_info_.window_rect);
  }

  fullscreen_ = fullscreen;

  if (fullscreen_) {
    // Set new window style and size.
    SetWindowLong(widget_->GetNativeView(), GWL_STYLE,
                  saved_window_info_.style & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(widget_->GetNativeView(), GWL_EXSTYLE,
                  saved_window_info_.ex_style & ~(WS_EX_DLGMODALFRAME |
                  WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    // On expand, if we're given a window_rect, grow to it, otherwise do
    // not resize.
    if (!for_metro) {
      MONITORINFO monitor_info;
      monitor_info.cbSize = sizeof(monitor_info);
      GetMonitorInfo(MonitorFromWindow(widget_->GetNativeView(),
                                       MONITOR_DEFAULTTONEAREST),
                     &monitor_info);
      gfx::Rect window_rect(monitor_info.rcMonitor);
      SetWindowPos(widget_->GetNativeView(), NULL,
                   window_rect.x(), window_rect.y(),
                   window_rect.width(), window_rect.height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
  } else {
    // Reset original window style and size.  The multiple window size/moves
    // here are ugly, but if SetWindowPos() doesn't redraw, the taskbar won't be
    // repainted.  Better-looking methods welcome.
    SetWindowLong(widget_->GetNativeView(), GWL_STYLE,
                  saved_window_info_.style);
    SetWindowLong(widget_->GetNativeView(), GWL_EXSTYLE,
                  saved_window_info_.ex_style);

    if (!for_metro) {
      // On restore, resize to the previous saved rect size.
      gfx::Rect new_rect(saved_window_info_.window_rect);
      SetWindowPos(widget_->GetNativeView(), NULL,
                   new_rect.x(), new_rect.y(),
                   new_rect.width(), new_rect.height(),
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    if (saved_window_info_.maximized)
      widget_->Maximize();
  }
}

}  // namespace views
