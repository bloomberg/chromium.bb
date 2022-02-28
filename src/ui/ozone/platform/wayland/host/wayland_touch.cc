// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_touch.h"

#include "base/time/time.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandTouch::WaylandTouch(wl_touch* touch,
                           WaylandConnection* connection,
                           Delegate* delegate)
    : obj_(touch), connection_(connection), delegate_(delegate) {
  static constexpr wl_touch_listener listener = {
      &Down, &Up, &Motion, &Frame, &Cancel,
  };

  wl_touch_add_listener(obj_.get(), &listener, this);
}

WaylandTouch::~WaylandTouch() {
  delegate_->OnTouchCancelEvent();
}

void WaylandTouch::Down(void* data,
                        wl_touch* obj,
                        uint32_t serial,
                        uint32_t time,
                        struct wl_surface* surface,
                        int32_t id,
                        wl_fixed_t x,
                        wl_fixed_t y) {
  if (!surface)
    return;

  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);

  touch->connection_->serial_tracker().UpdateSerial(wl::SerialType::kTouchPress,
                                                    serial);

  WaylandWindow* window = wl::RootWindowFromWlSurface(surface);
  gfx::PointF location(wl_fixed_to_double(x), wl_fixed_to_double(y));
  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchPressEvent(window, location, timestamp, id);
}

void WaylandTouch::Up(void* data,
                      wl_touch* obj,
                      uint32_t serial,
                      uint32_t time,
                      int32_t id) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);

  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchReleaseEvent(timestamp, id);

  // Reset kTouchPress serial only after dispatching touch-up event, so popups
  // may detect if they were triggered by a tap gesture, and avoid grab in such
  // case, which, per the spec, is illegal and may lead to instant popup
  // dismissal by the compositor.
  touch->connection_->serial_tracker().ResetSerial(wl::SerialType::kTouchPress);
}

void WaylandTouch::Motion(void* data,
                          wl_touch* obj,
                          uint32_t time,
                          int32_t id,
                          wl_fixed_t x,
                          wl_fixed_t y) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);

  gfx::PointF location(wl_fixed_to_double(x), wl_fixed_to_double(y));
  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchMotionEvent(location, timestamp, id);
}

void WaylandTouch::Cancel(void* data, wl_touch* obj) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  touch->delegate_->OnTouchCancelEvent();
}

void WaylandTouch::Frame(void* data, wl_touch* obj) {}

}  // namespace ui
