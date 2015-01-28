// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/evdev/events_ozone_evdev_export.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace base {
class TimeDelta;
}

namespace gfx {
class PointF;
class Vector2dF;
}

namespace ui {

class Event;

typedef base::Callback<void(scoped_ptr<Event>)> EventDispatchCallback;

struct EVENTS_OZONE_EVDEV_EXPORT KeyEventParams {
  KeyEventParams(int device_id, unsigned int code, bool down);
  KeyEventParams(const KeyEventParams& other);
  ~KeyEventParams();

  int device_id;
  unsigned int code;
  bool down;
};

typedef base::Callback<void(const KeyEventParams& params)>
    KeyEventDispatchCallback;

struct EVENTS_OZONE_EVDEV_EXPORT MouseMoveEventParams {
  MouseMoveEventParams(int device_id, const gfx::PointF& location);
  MouseMoveEventParams(const MouseMoveEventParams& other);
  ~MouseMoveEventParams();

  int device_id;
  gfx::PointF location;
};

typedef base::Callback<void(const MouseMoveEventParams& params)>
    MouseMoveEventDispatchCallback;

struct EVENTS_OZONE_EVDEV_EXPORT MouseButtonEventParams {
  MouseButtonEventParams(int device_id,
                         const gfx::PointF& location,
                         unsigned int button,
                         bool down,
                         bool allow_remap);
  MouseButtonEventParams(const MouseButtonEventParams& other);
  ~MouseButtonEventParams();

  int device_id;
  gfx::PointF location;
  unsigned int button;
  bool down;
  bool allow_remap;
};

typedef base::Callback<void(const MouseButtonEventParams& params)>
    MouseButtonEventDispatchCallback;

struct EVENTS_OZONE_EVDEV_EXPORT TouchEventParams {
  TouchEventParams(int device_id,
                   int touch_id,
                   EventType type,
                   const gfx::PointF& location,
                   const gfx::Vector2dF& radii,
                   float pressure,
                   const base::TimeDelta& timestamp);
  TouchEventParams(const TouchEventParams& other);
  ~TouchEventParams();

  int device_id;
  int touch_id;
  EventType type;
  gfx::PointF location;
  gfx::Vector2dF radii;
  float pressure;
  base::TimeDelta timestamp;
};

typedef base::Callback<void(const TouchEventParams& params)>
    TouchEventDispatchCallback;

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
