// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/tablet_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"

namespace ui {

TabletEventConverterEvdev::TabletEventConverterEvdev(
    int fd,
    base::FilePath path,
    int id,
    InputDeviceType type,
    CursorDelegateEvdev* cursor,
    const EventDeviceInfo& info,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd, path, id, type),
      cursor_(cursor),
      dispatcher_(dispatcher),
      stylus_(0),
      abs_value_dirty_(false) {
  x_abs_min_ = info.GetAbsMinimum(ABS_X);
  x_abs_range_ = info.GetAbsMaximum(ABS_X) - x_abs_min_ + 1;
  y_abs_min_ = info.GetAbsMinimum(ABS_Y);
  y_abs_range_ = info.GetAbsMaximum(ABS_Y) - y_abs_min_ + 1;
}

TabletEventConverterEvdev::~TabletEventConverterEvdev() {
  Stop();
  close(fd_);
}

void TabletEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[4];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  if (ignore_events_)
    return;

  DCHECK_EQ(read_size % sizeof(*inputs), 0u);
  ProcessEvents(inputs, read_size / sizeof(*inputs));
}

void TabletEventConverterEvdev::ProcessEvents(const input_event* inputs,
                                              int count) {
  for (int i = 0; i < count; ++i) {
    const input_event& input = inputs[i];
    switch (input.type) {
      case EV_KEY:
        ConvertKeyEvent(input);
        break;
      case EV_ABS:
        ConvertAbsEvent(input);
        break;
      case EV_SYN:
        FlushEvents(input);
        break;
    }
  }
}

void TabletEventConverterEvdev::ConvertKeyEvent(const input_event& input) {
  // Only handle other events if we have a stylus in proximity
  if (input.code >= BTN_TOOL_PEN && input.code <= BTN_TOOL_LENS) {
    if (input.value == 1)
      stylus_ = input.code;
    else if (input.value == 0)
      stylus_ = 0;
    else
      LOG(WARNING) << "Unexpected value: " << input.value
                   << " for code: " << input.code;
  }

  if (input.code >= BTN_TOUCH && input.code <= BTN_STYLUS2) {
    DispatchMouseButton(input);
    return;
  }
}

void TabletEventConverterEvdev::ConvertAbsEvent(const input_event& input) {
  if (!cursor_)
    return;

  switch (input.code) {
    case ABS_X:
      x_abs_location_ = input.value;
      abs_value_dirty_ = true;
      break;
    case ABS_Y:
      y_abs_location_ = input.value;
      abs_value_dirty_ = true;
      break;
  }
}

void TabletEventConverterEvdev::UpdateCursor() {
  gfx::Rect confined_bounds = cursor_->GetCursorConfinedBounds();

  int x =
      ((x_abs_location_ - x_abs_min_) * confined_bounds.width()) / x_abs_range_;
  int y = ((y_abs_location_ - y_abs_min_) * confined_bounds.height()) /
          y_abs_range_;

  x += confined_bounds.x();
  y += confined_bounds.y();

  cursor_->MoveCursorTo(gfx::PointF(x, y));
}

void TabletEventConverterEvdev::DispatchMouseButton(const input_event& input) {
  if (!cursor_)
    return;

  unsigned int button;
  // These are the same as X11 behaviour
  if (input.code == BTN_TOUCH)
    button = BTN_LEFT;
  else if (input.code == BTN_STYLUS2)
    button = BTN_RIGHT;
  else if (input.code == BTN_STYLUS)
    button = BTN_MIDDLE;
  else
    return;

  if (abs_value_dirty_) {
    UpdateCursor();
    abs_value_dirty_ = false;
  }

  bool down = input.value;

  dispatcher_->DispatchMouseButtonEvent(MouseButtonEventParams(
      id_, cursor_->GetLocation(), button, down, false /* allow_remap */,
      TimeDeltaFromInputEvent(input)));
}

void TabletEventConverterEvdev::FlushEvents(const input_event& input) {
  if (!cursor_)
    return;

  // Prevent propagation of invalid data on stylus lift off
  if (stylus_ == 0) {
    abs_value_dirty_ = false;
    return;
  }

  if (!abs_value_dirty_)
    return;

  UpdateCursor();

  dispatcher_->DispatchMouseMoveEvent(MouseMoveEventParams(
      id_, cursor_->GetLocation(), TimeDeltaFromInputEvent(input)));

  abs_value_dirty_ = false;
}

}  // namespace ui
