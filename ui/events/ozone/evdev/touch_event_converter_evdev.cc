// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include <cmath>
#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_pump_ozone.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/event_factory_ozone.h"
#include "ui/gfx/screen.h"

namespace {

// Number is determined empirically.
// TODO(rjkroege): Configure this per device.
const float kFingerWidth = 25.f;

}  // namespace

namespace ui {

TouchEventConverterEvdev::TouchEventConverterEvdev(int fd,
                                                   base::FilePath path,
                                                   const EventDeviceInfo& info)
    : pressure_min_(info.GetAbsMinimum(ABS_MT_PRESSURE)),
      pressure_max_(info.GetAbsMaximum(ABS_MT_PRESSURE)),
      x_scale_(1.),
      y_scale_(1.),
      x_min_(info.GetAbsMinimum(ABS_MT_POSITION_X)),
      x_max_(info.GetAbsMaximum(ABS_MT_POSITION_X)),
      y_min_(info.GetAbsMinimum(ABS_MT_POSITION_Y)),
      y_max_(info.GetAbsMaximum(ABS_MT_POSITION_Y)),
      current_slot_(0),
      fd_(fd),
      path_(path) {
  Init();
}

TouchEventConverterEvdev::~TouchEventConverterEvdev() {
  Stop();
  close(fd_);
}

void TouchEventConverterEvdev::Init() {
  gfx::Screen *screen = gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  if (!screen)
    return;  // No scaling.
  gfx::Display display = screen->GetPrimaryDisplay();
  gfx::Size size = display.GetSizeInPixel();

  x_scale_ = (double)size.width() / (x_max_ - x_min_);
  y_scale_ = (double)size.height() / (y_max_ - y_min_);
  VLOG(1) << "touch scaling x_scale=" << x_scale_ << " y_scale=" << y_scale_;
}

void TouchEventConverterEvdev::Start() {
  base::MessagePumpOzone::Current()->WatchFileDescriptor(
      fd_, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
}

void TouchEventConverterEvdev::Stop() {
  controller_.StopWatchingFileDescriptor();
}

void TouchEventConverterEvdev::OnFileCanWriteWithoutBlocking(int /* fd */) {
  // Read-only file-descriptors.
  NOTREACHED();
}

void TouchEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[MAX_FINGERS * 6 + 1];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size < 0) {
    if (errno == EINTR || errno == EAGAIN)
      return;
    if (errno != ENODEV)
      PLOG(ERROR) << "error reading device " << path_.value();
    Stop();
    return;
  }

  ScopedVector<ui::TouchEvent> touch_events;
  for (unsigned i = 0; i < read_size / sizeof(*inputs); i++) {
    const input_event& input = inputs[i];
    if (input.type == EV_ABS) {
      switch (input.code) {
        case ABS_MT_TOUCH_MAJOR:
          altered_slots_.set(current_slot_);
          events_[current_slot_].major_ = input.value;
          break;
        case ABS_X:
        case ABS_MT_POSITION_X:
          altered_slots_.set(current_slot_);
          events_[current_slot_].x_ = roundf(input.value * x_scale_);
          break;
        case ABS_Y:
        case ABS_MT_POSITION_Y:
          altered_slots_.set(current_slot_);
          events_[current_slot_].y_ = roundf(input.value * y_scale_);
          break;
        case ABS_MT_TRACKING_ID:
          altered_slots_.set(current_slot_);
          if (input.value < 0) {
            events_[current_slot_].type_ = ET_TOUCH_RELEASED;
          } else {
            events_[current_slot_].finger_ = input.value;
            events_[current_slot_].type_ = ET_TOUCH_PRESSED;
          }
          break;
        case ABS_MT_PRESSURE:
        case ABS_PRESSURE:
          altered_slots_.set(current_slot_);
          events_[current_slot_].pressure_ = input.value - pressure_min_;
          events_[current_slot_].pressure_ /= pressure_max_ - pressure_min_;
          break;
        case ABS_MT_SLOT:
          current_slot_ = input.value;
          altered_slots_.set(current_slot_);
          break;
        default:
          NOTIMPLEMENTED() << "invalid code for EV_ABS: " << input.code;
      }
    } else if (input.type == EV_SYN) {
      switch (input.code) {
        case SYN_REPORT:
          for (int j = 0; j < MAX_FINGERS; j++) {
            if (altered_slots_[j]) {
              // TODO(rjkroege): Support elliptical finger regions.
              touch_events.push_back(new TouchEvent(
                  events_[j].type_,
                  gfx::Point(std::min(x_max_, events_[j].x_),
                             std::min(y_max_, events_[j].y_)),
                  /* flags */ 0,
                  /* touch_id */ j,
                  base::TimeDelta::FromMicroseconds(
                      input.time.tv_sec * 1000000 + input.time.tv_usec),
                  events_[j].pressure_ * kFingerWidth,
                  events_[j].pressure_ * kFingerWidth,
                  /* angle */ 0.,
                  events_[j].pressure_));

              // Subsequent events for this finger will be touch-move until it
              // is released.
              events_[j].type_ = ET_TOUCH_MOVED;
            }
          }
          altered_slots_.reset();
          break;
        case SYN_MT_REPORT:
        case SYN_CONFIG:
        case SYN_DROPPED:
          NOTIMPLEMENTED() << "invalid code for EV_SYN: " << input.code;
          break;
      }
    } else if (input.type == EV_KEY) {
      switch (input.code) {
        case BTN_TOUCH:
          break;
        default:
          NOTIMPLEMENTED() << "invalid code for EV_KEY: " << input.code;
      }
    } else {
      NOTIMPLEMENTED() << "invalid type: " << input.type;
    }
  }
  for (ScopedVector<ui::TouchEvent>::iterator iter = touch_events.begin();
       iter != touch_events.end(); ++iter) {
    DispatchEventToCallback(*iter);
  }
}

}  // namespace ui
