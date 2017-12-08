// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

// Stub implementations of platform-specific methods in events_util.h, built
// on platforms that currently do not have a complete implementation of events.

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

base::TimeTicks EventTimeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return base::TimeTicks();
}

gfx::PointF EventLocationFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::PointF();
}

gfx::Point EventSystemLocationFromNative(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

int EventButtonFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetMousePointerDetailsFromNative(
    const base::NativeEvent& native_event) {
  return PointerDetails(EventPointerType::POINTER_TYPE_MOUSE);
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Vector2d();
}

base::NativeEvent CopyNativeEvent(const base::NativeEvent& event) {
  NOTIMPLEMENTED() <<
      "Don't know how to copy base::NativeEvent for this platform";
  return NULL;
}

void ReleaseCopiedNativeEvent(const base::NativeEvent& event) {
}

void ClearTouchIdIfReleased(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

int GetTouchId(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetTouchPointerDetailsFromNative(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::POINTER_TYPE_UNKNOWN,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f,
                        /* twist */ 0.f,
                        /* tilt_x */ 0.f,
                        /* tilt_y */ 0.f);
}

bool GetScrollOffsets(const base::NativeEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  NOTIMPLEMENTED();
  return false;
}

bool GetFlingData(const base::NativeEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return static_cast<KeyboardCode>(0);
}

DomCode CodeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return DomCode::NONE;
}

bool IsCharFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return false;
}

uint32_t WindowsKeycodeFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t TextFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t UnmodifiedTextFromNative(const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}


}  // namespace ui
