// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

#include <vector>

#include "base/time/time.h"
#include "ui/events/pointer_details.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandWindow;

class WaylandTouch {
 public:
  class Delegate;

  WaylandTouch(wl_touch* touch,
               WaylandConnection* connection,
               Delegate* delegate);

  WaylandTouch(const WaylandTouch&) = delete;
  WaylandTouch& operator=(const WaylandTouch&) = delete;

  ~WaylandTouch();

 private:
  // wl_touch_listener
  static void Down(void* data,
                   wl_touch* obj,
                   uint32_t serial,
                   uint32_t time,
                   struct wl_surface* surface,
                   int32_t id,
                   wl_fixed_t x,
                   wl_fixed_t y);
  static void Up(void* data,
                 wl_touch* obj,
                 uint32_t serial,
                 uint32_t time,
                 int32_t id);
  static void Motion(void* data,
                     wl_touch* obj,
                     uint32_t time,
                     int32_t id,
                     wl_fixed_t x,
                     wl_fixed_t y);
  static void Cancel(void* data, wl_touch* obj);
  static void Frame(void* data, wl_touch* obj);

  void SetupStylus();

  // zcr_touch_stylus_v2_listener
  static void Tool(void* data,
                   struct zcr_touch_stylus_v2* obj,
                   uint32_t id,
                   uint32_t type);
  static void Force(void* data,
                    struct zcr_touch_stylus_v2* obj,
                    uint32_t time,
                    uint32_t id,
                    wl_fixed_t force);
  static void Tilt(void* data,
                   struct zcr_touch_stylus_v2* obj,
                   uint32_t time,
                   uint32_t id,
                   wl_fixed_t tilt_x,
                   wl_fixed_t tilt_y);

  wl::Object<wl_touch> obj_;
  wl::Object<zcr_touch_stylus_v2> zcr_touch_stylus_v2_;
  WaylandConnection* const connection_;
  Delegate* const delegate_;
};

class WaylandTouch::Delegate {
 public:
  enum class EventDispatchPolicy {
    kImmediate,
    kOnFrame,
  };
  virtual void OnTouchPressEvent(WaylandWindow* window,
                                 const gfx::PointF& location,
                                 base::TimeTicks timestamp,
                                 PointerId id,
                                 EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchReleaseEvent(base::TimeTicks timestamp,
                                   PointerId id,
                                   EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchMotionEvent(const gfx::PointF& location,
                                  base::TimeTicks timestamp,
                                  PointerId id,
                                  EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchCancelEvent() = 0;
  virtual void OnTouchFrame() = 0;
  virtual void OnTouchFocusChanged(WaylandWindow* window) = 0;
  virtual std::vector<PointerId> GetActiveTouchPointIds() = 0;
  virtual const WaylandWindow* GetTouchTarget(PointerId id) const = 0;
  virtual void OnTouchStylusToolChanged(PointerId pointer_id,
                                        EventPointerType pointer_type) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
