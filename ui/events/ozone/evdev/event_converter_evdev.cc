// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>

#include "ui/events/ozone/evdev/event_converter_evdev.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/devices/input_device.h"

namespace ui {

EventConverterEvdev::EventConverterEvdev(int fd,
                                         const base::FilePath& path,
                                         int id,
                                         InputDeviceType type)
    : fd_(fd), path_(path), id_(id), type_(type), ignore_events_(false) {
}

EventConverterEvdev::~EventConverterEvdev() {
  Stop();
}

void EventConverterEvdev::Start() {
  base::MessageLoopForUI::current()->WatchFileDescriptor(
      fd_, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
}

void EventConverterEvdev::Stop() {
  controller_.StopWatchingFileDescriptor();

  OnStopped();
}

void EventConverterEvdev::OnStopped() {
}

void EventConverterEvdev::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

bool EventConverterEvdev::HasKeyboard() const {
  return false;
}

bool EventConverterEvdev::HasMouse() const {
  return false;
}

bool EventConverterEvdev::HasTouchpad() const {
  return false;
}

bool EventConverterEvdev::HasTouchscreen() const {
  return false;
}

bool EventConverterEvdev::HasCapsLockLed() const {
  return false;
}

gfx::Size EventConverterEvdev::GetTouchscreenSize() const {
  NOTREACHED();
  return gfx::Size();
}

int EventConverterEvdev::GetTouchPoints() const {
  NOTREACHED();
  return 0;
}

void EventConverterEvdev::SetAllowedKeys(
    scoped_ptr<std::set<DomCode>> allowed_keys) {
  NOTREACHED();
}

void EventConverterEvdev::AllowAllKeys() {
  NOTREACHED();
}

void EventConverterEvdev::SetCapsLockLed(bool enabled) {
  if (!HasCapsLockLed())
    return;

  input_event events[2];
  memset(&events, 0, sizeof(events));

  events[0].type = EV_LED;
  events[0].code = LED_CAPSL;
  events[0].value = enabled;

  events[1].type = EV_SYN;
  events[1].code = SYN_REPORT;
  events[1].value = 0;

  ssize_t written = write(fd_, events, sizeof(events));

  if (written < 0) {
    if (errno != ENODEV)
      PLOG(ERROR) << "cannot set leds for " << path_.value() << ":";
    Stop();
  } else if (written != sizeof(events)) {
    LOG(ERROR) << "short write setting leds for " << path_.value();
    Stop();
  }
}

base::TimeDelta EventConverterEvdev::TimeDeltaFromInputEvent(
    const input_event& event) {
  return base::TimeDelta::FromMicroseconds(event.time.tv_sec * 1000000 +
                                           event.time.tv_usec);
}
}  // namespace ui
