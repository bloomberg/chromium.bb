// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_touch.h"

#include <stylus-unstable-v2-client-protocol.h>

#include "base/logging.h"
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

  SetupStylus();
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
  gfx::PointF location = touch->connection_->MaybeConvertLocation(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), window);
  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchPressEvent(window, location, timestamp, id,
                                      Delegate::EventDispatchPolicy::kOnFrame);
}

void WaylandTouch::Up(void* data,
                      wl_touch* obj,
                      uint32_t serial,
                      uint32_t time,
                      int32_t id) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);

  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchReleaseEvent(
      timestamp, id, Delegate::EventDispatchPolicy::kOnFrame);
}

void WaylandTouch::Motion(void* data,
                          wl_touch* obj,
                          uint32_t time,
                          int32_t id,
                          wl_fixed_t x,
                          wl_fixed_t y) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  const WaylandWindow* target = touch->delegate_->GetTouchTarget(id);
  if (!target) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }
  gfx::PointF location = touch->connection_->MaybeConvertLocation(
      gfx::PointF(wl_fixed_to_double(x), wl_fixed_to_double(y)), target);
  base::TimeTicks timestamp = base::TimeTicks() + base::Milliseconds(time);
  touch->delegate_->OnTouchMotionEvent(location, timestamp, id,
                                       Delegate::EventDispatchPolicy::kOnFrame);
}

void WaylandTouch::Cancel(void* data, wl_touch* obj) {
  WaylandTouch* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  touch->delegate_->OnTouchCancelEvent();
}

void WaylandTouch::Frame(void* data, wl_touch* obj) {
  auto* touch = static_cast<WaylandTouch*>(data);
  DCHECK(touch);
  touch->delegate_->OnTouchFrame();
}

void WaylandTouch::SetupStylus() {
  auto* stylus_v2 = connection_->stylus_v2();
  if (!stylus_v2)
    return;

  zcr_touch_stylus_v2_.reset(
      zcr_stylus_v2_get_touch_stylus(stylus_v2, obj_.get()));

  static zcr_touch_stylus_v2_listener kTouchStylusV2Listener = {&Tool, &Force,
                                                                &Tilt};
  zcr_touch_stylus_v2_add_listener(zcr_touch_stylus_v2_.get(),
                                   &kTouchStylusV2Listener, this);
}

// static
void WaylandTouch::Tool(void* data,
                        struct zcr_touch_stylus_v2* obj,
                        uint32_t id,
                        uint32_t stylus_type) {
  auto* pointer = static_cast<WaylandTouch*>(data);

  ui::EventPointerType pointer_type = ui::EventPointerType::kTouch;
  switch (stylus_type) {
    case ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_PEN:
      pointer_type = EventPointerType::kPen;
      break;
    case ZCR_TOUCH_STYLUS_V2_TOOL_TYPE_ERASER:
      pointer_type = ui::EventPointerType::kEraser;
      break;
    case ZCR_POINTER_STYLUS_V2_TOOL_TYPE_TOUCH:
      break;
  }

  pointer->delegate_->OnTouchStylusToolChanged(id, pointer_type);
}

// static
void WaylandTouch::Force(void* data,
                         struct zcr_touch_stylus_v2* obj,
                         uint32_t time,
                         uint32_t id,
                         wl_fixed_t force) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandTouch::Tilt(void* data,
                        struct zcr_touch_stylus_v2* obj,
                        uint32_t time,
                        uint32_t id,
                        wl_fixed_t tilt_x,
                        wl_fixed_t tilt_y) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
