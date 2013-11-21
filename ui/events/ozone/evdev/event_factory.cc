// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include "base/strings/stringprintf.h"
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

EventFactoryEvdev::~EventFactoryEvdev() {}

void EventFactoryEvdev::StartProcessingEvents() {
  // The number of devices in the directory is unknown without reading
  // the contents of the directory. Further, with hot-plugging,  the entries
  // might decrease during the execution of this loop. So exciting from the
  // loop on the first failure of open below is both cheaper and more
  // reliable.
  for (int id = 0; true; id++) {
    std::string path = base::StringPrintf("/dev/input/event%d", id);
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      DLOG(ERROR) << "Cannot open '" << path << "': " << strerror(errno);
      break;
    }

    EventDeviceInfo devinfo;
    if (!devinfo.Initialize(fd)) {
      DLOG(ERROR) << "failed to get device information for " << path;
      close(fd);
      continue;
    }

    if (IsTouchPad(devinfo)) {
      LOG(WARNING) << "touchpad device not supported: " << path;
      close(fd);
      continue;
    }

    scoped_ptr<EventConverterOzone> converter;
    // TODO(rjkroege) Add more device types. Support hot-plugging.
    if (IsTouchScreen(devinfo))
      converter.reset(new TouchEventConverterEvdev(fd, id));
    else if (devinfo.HasEventType(EV_KEY))
      converter.reset(new KeyEventConverterEvdev(fd, id, &modifiers_));

    if (converter) {
      AddEventConverter(fd, converter.Pass());
    } else {
      close(fd);
    }
  }
}

}  // namespace ui
