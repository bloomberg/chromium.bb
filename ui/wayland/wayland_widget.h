// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_WIDGET_H_
#define UI_WAYLAND_WAYLAND_WIDGET_H_

#include "ui/wayland/events/wayland_event.h"

namespace ui {

// WaylandWidget is an interface for processing Wayland events.
class WaylandWidget {
 public:
  virtual ~WaylandWidget() {}

  virtual void OnMotionNotify(WaylandEvent event) = 0;
  virtual void OnButtonNotify(WaylandEvent event) = 0;
  virtual void OnKeyNotify(WaylandEvent event) = 0;
  virtual void OnPointerFocus(WaylandEvent event) = 0;
  virtual void OnKeyboardFocus(WaylandEvent event) = 0;

  virtual void OnGeometryChange(WaylandEvent event) = 0;
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_WIDGET_H_
