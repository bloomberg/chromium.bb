// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_WIDGET_H_
#define UI_WAYLAND_WAYLAND_WIDGET_H_

namespace base {
namespace wayland {
union WaylandEvent;
}
}

namespace ui {

// WaylandWidget is an interface for processing Wayland events.
class WaylandWidget {
 public:
  virtual ~WaylandWidget() {}

  virtual void OnMotionNotify(base::wayland::WaylandEvent event) = 0;
  virtual void OnButtonNotify(base::wayland::WaylandEvent event) = 0;
  virtual void OnKeyNotify(base::wayland::WaylandEvent event) = 0;
  virtual void OnPointerFocus(base::wayland::WaylandEvent event) = 0;
  virtual void OnKeyboardFocus(base::wayland::WaylandEvent event) = 0;

  virtual void OnGeometryChange(base::wayland::WaylandEvent event) = 0;
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_WIDGET_H_
