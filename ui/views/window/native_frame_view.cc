// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view.h"

#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/views/widget/native_widget_win.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, public:

NativeFrameView::NativeFrameView(Widget* frame)
    : NonClientFrameView(),
      frame_(frame) {
}

NativeFrameView::~NativeFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, NonClientFrameView overrides:

gfx::Rect NativeFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect NativeFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
#if defined(OS_WIN) && !defined(USE_AURA)
  RECT rect = client_bounds.ToRECT();
  NativeWidgetWin* widget_win =
      static_cast<NativeWidgetWin*>(frame_->native_widget());
  AdjustWindowRectEx(&rect, widget_win->window_style(), FALSE,
                     widget_win->window_ex_style());
  return gfx::Rect(rect);
#else
  // TODO(sad):
  return client_bounds;
#endif
}

int NativeFrameView::NonClientHitTest(const gfx::Point& point) {
  return frame_->client_view()->NonClientHitTest(point);
}

void NativeFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // Nothing to do, we use the default window mask.
}

void NativeFrameView::ResetWindowControls() {
  // Nothing to do.
}

void NativeFrameView::UpdateWindowIcon() {
  // Nothing to do.
}

gfx::Size NativeFrameView::GetPreferredSize() {
  return frame_->client_view()->GetPreferredSize();
}

}  // namespace views
