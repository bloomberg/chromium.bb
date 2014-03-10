// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

float GetDeviceScaleForNativeView(views::Widget* widget) {
  float device_scale = 1.0f;
  // The following code should work on other platforms as well. But we do not
  // yet care about device scale factor on other platforms. So to keep drag and
  // drop behavior on other platforms un-touched, we wrap this in the #if guard.
  if (widget && widget->GetNativeView()) {
    gfx::NativeView view = widget->GetNativeView();
    gfx::Display display = gfx::Screen::GetScreenFor(view)->
        GetDisplayNearestWindow(view);
    device_scale = display.device_scale_factor();
  }
  return device_scale;
}

namespace views {

void RunShellDrag(gfx::NativeView view,
                  const ui::OSExchangeData& data,
                  const gfx::Point& location,
                  int operation,
                  ui::DragDropTypes::DragEventSource source) {
  gfx::Point root_location(location);
  aura::Window* root_window = view->GetRootWindow();
  aura::Window::ConvertPointToTarget(view, root_window, &root_location);
  if (aura::client::GetDragDropClient(root_window)) {
    aura::client::GetDragDropClient(root_window)->StartDragAndDrop(
        data, root_window, view, root_location, operation, source);
  }
}

gfx::Canvas* GetCanvasForDragImage(views::Widget* widget,
                                   const gfx::Size& canvas_size) {
  float device_scale = GetDeviceScaleForNativeView(widget);
  return new gfx::Canvas(canvas_size, device_scale, false);
}

}  // namespace views
