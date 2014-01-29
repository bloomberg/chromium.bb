// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include "base/debug/trace_event.h"
#include "base/files/file_enumerator.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/key_event_converter.h"
#include "ui/events/ozone/evdev/touch_event_converter.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

namespace {

bool IsTouchPad(const EventDeviceInfo& devinfo) {
  if (!devinfo.HasEventType(EV_ABS))
    return false;

  return devinfo.HasKeyEvent(BTN_LEFT) || devinfo.HasKeyEvent(BTN_MIDDLE) ||
         devinfo.HasKeyEvent(BTN_RIGHT) || devinfo.HasKeyEvent(BTN_TOOL_FINGER);
}

bool IsTouchScreen(const EventDeviceInfo& devinfo) {
  return devinfo.HasEventType(EV_ABS) && !IsTouchPad(devinfo);
}

}  // namespace

EventFactoryEvdev::EventFactoryEvdev() {}

EventFactoryEvdev::~EventFactoryEvdev() { STLDeleteValues(&converters_); }

void EventFactoryEvdev::AttachInputDevice(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "AttachInputDevice", "path", path.value());

  int fd = open(path.value().c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    PLOG(ERROR) << "Cannot open '" << path.value();
    return;
  }

  EventDeviceInfo devinfo;
  if (!devinfo.Initialize(fd)) {
    LOG(ERROR) << "failed to get device information for " << path.value();
    close(fd);
    return;
  }

  if (IsTouchPad(devinfo)) {
    LOG(WARNING) << "touchpad device not supported: " << path.value();
    close(fd);
    return;
  }

  // TODO(spang) Add more device types. Support hot-plugging.
  scoped_ptr<EventConverterOzone> converter;
  if (IsTouchScreen(devinfo))
    converter.reset(new TouchEventConverterEvdev(fd, path));
  else if (devinfo.HasEventType(EV_KEY))
    converter.reset(new KeyEventConverterEvdev(fd, path, &modifiers_));

  if (converter) {
    converters_[path] = converter.release();
  } else {
    close(fd);
  }
}

void EventFactoryEvdev::StartProcessingEvents() {
  base::ThreadRestrictions::AssertIOAllowed();

  base::FileEnumerator file_enum(base::FilePath("/dev/input"),
                                 false,
                                 base::FileEnumerator::FILES,
                                 "event*[0-9]");
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next())
    AttachInputDevice(path);
}

}  // namespace ui
