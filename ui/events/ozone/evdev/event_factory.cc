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
#include "ui/events/ozone/evdev/key_event_converter.h"
#include "ui/events/ozone/evdev/touch_event_converter.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

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
    size_t evtype = 0;
    COMPILE_ASSERT(sizeof(evtype) * 8 >= EV_MAX, evtype_wide_enough);
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evtype)), &evtype) == -1) {
      DLOG(ERROR) << "failed ioctl EVIOCGBIT 0" << path;
      close(fd);
      continue;
    }

    scoped_ptr<EventConverterOzone> converter;
    // TODO(rjkroege) Add more device types. Support hot-plugging.
    if (evtype & (1 << EV_ABS))
      converter.reset(new TouchEventConverterEvdev(fd, id));
    else if (evtype & (1 << EV_KEY))
      converter.reset(new KeyEventConverterEvdev(&modifiers_));

    if (converter) {
      AddEventConverter(fd, converter.Pass());
    } else {
      close(fd);
    }
  }
}

}  // namespace ui
