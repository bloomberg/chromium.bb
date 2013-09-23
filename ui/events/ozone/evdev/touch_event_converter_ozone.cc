// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_event_converter_ozone.h"

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
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_ozone.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"

namespace {

// Number is determined empirically.
// TODO(rjkroege): Configure this per device.
const float kFingerWidth = 25.f;

}  // namespace

namespace ui {

TouchEventConverterOzone::TouchEventConverterOzone(int fd, int id)
    : pressure_min_(0),
      pressure_max_(0),
      x_scale_(1.),
      y_scale_(1.),
      x_max_(std::numeric_limits<int>::max()),
      y_max_(std::numeric_limits<int>::max()),
      current_slot_(0),
      fd_(fd),
      id_(id) {
  Init();
}

TouchEventConverterOzone::~TouchEventConverterOzone() {
  if (close(fd_) < 0)
    DLOG(WARNING) << "failed close on /dev/input/event" << id_;
}

void TouchEventConverterOzone::Init() {
  input_absinfo abs = {};
  if (ioctl(fd_, EVIOCGABS(ABS_MT_SLOT), &abs) != -1) {
    CHECK_GE(abs.maximum, abs.minimum);
    CHECK_GE(abs.minimum, 0);
  } else {
    DLOG(WARNING) << "failed ioctl EVIOCGABS ABS_MT_SLOT event" << id_;
  }
  if (ioctl(fd_, EVIOCGABS(ABS_MT_PRESSURE), &abs) != -1) {
    pressure_min_ = abs.minimum;
    pressure_max_ = abs.maximum;
  } else {
    DLOG(WARNING) << "failed ioctl EVIOCGABS ABS_MT_PRESSURE event" << id_;
  }
  int x_min = 0, x_max = 0;
  if (ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &abs) != -1) {
    x_min = abs.minimum;
    x_max = abs.maximum;
  } else {
    LOG(WARNING) << "failed ioctl EVIOCGABS ABS_X event" << id_;
  }
  int y_min = 0, y_max = 0;
  if (ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &abs) != -1) {
    y_min = abs.minimum;
    y_max = abs.maximum;
  } else {
    LOG(WARNING) << "failed ioctl EVIOCGABS ABS_Y event" << id_;
  }
  if (x_max && y_max && gfx::SurfaceFactoryOzone::GetInstance()) {
    const char* display =
        gfx::SurfaceFactoryOzone::GetInstance()->DefaultDisplaySpec();
    int screen_width, screen_height;
    int sc = sscanf(display, "%dx%d", &screen_width, &screen_height);
    if (sc == 2) {
      x_scale_ = (double)screen_width / (x_max - x_min);
      y_scale_ = (double)screen_height / (y_max - y_min);
      x_max_ = screen_width - 1;
      y_max_ = screen_height - 1;
      LOG(INFO) << "touch input x_scale=" << x_scale_
                << " y_scale=" << y_scale_;
    } else {
      LOG(WARNING) << "malformed display spec from "
                   << "SurfaceFactoryOzone::DefaultDisplaySpec";
    }
  }
}

void TouchEventConverterOzone::OnFileCanWriteWithoutBlocking(int /* fd */) {
  // Read-only file-descriptors.
  NOTREACHED();
}

void TouchEventConverterOzone::OnFileCanReadWithoutBlocking(int fd) {
  input_event inputs[MAX_FINGERS * 6 + 1];
  ssize_t read_size = read(fd, inputs, sizeof(inputs));
  if (read_size <= 0)
    return;

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
          NOTREACHED();
      }
    } else if (input.type == EV_SYN) {
      switch (input.code) {
        case SYN_REPORT:
          for (int j = 0; j < MAX_FINGERS; j++) {
            if (altered_slots_[j]) {
              // TODO(rjkroege): Support elliptical finger regions.
              scoped_ptr<TouchEvent> tev(new TouchEvent(
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
              DispatchEvent(tev.PassAs<ui::Event>());

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
          NOTREACHED() << "SYN_MT events not supported.";
          break;
      }
    } else {
      NOTREACHED();
    }
  }
}

}  // namespace ui
