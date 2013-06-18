// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ozone/event_factory_ozone.h"

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include "base/message_loop/message_pump_ozone.h"
#include "base/strings/stringprintf.h"
#include "ui/base/ozone/key_event_converter_ozone.h"
#include "ui/base/ozone/touch_event_converter_ozone.h"

namespace ui {

EventFactoryOzone::EventFactoryOzone() {}

EventFactoryOzone::~EventFactoryOzone() {
  for (unsigned i = 0; i < fd_controllers_.size(); i++) {
    fd_controllers_[i]->StopWatchingFileDescriptor();
  }
}

void EventFactoryOzone::CreateEvdevWatchers() {
  // The number of devices in the directory is unknown without reading
  // the contents of the directory. Further, with hot-plugging,  the entries
  // might decrease during the execution of this loop. So exciting from the
  // loop on the first failure of open below is both cheaper and more
  // reliable.
  for (int id = 0; true; id++) {
    std::string path = base::StringPrintf("/dev/input/event%d", id);
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
      break;
    size_t evtype = 0;
    COMPILE_ASSERT(sizeof(evtype) * 8 >= EV_MAX, evtype_wide_enough);
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evtype)), &evtype) == -1) {
      DLOG(ERROR) << "failed ioctl EVIOCGBIT 0" << path;
      close(fd);
      continue;
    }

    EventConverterOzone* watcher = NULL;
    // TODO(rjkroege) Add more device types. Support hot-plugging.
    if (evtype & (1 << EV_ABS))
      watcher = new TouchEventConverterOzone(fd, id);
    else if (evtype & (1 << EV_KEY))
      watcher = new KeyEventConverterOzone();

    if (watcher) {
      base::MessagePumpLibevent::FileDescriptorWatcher* controller =
          new base::MessagePumpLibevent::FileDescriptorWatcher();
      base::MessagePumpOzone::Current()->WatchFileDescriptor(
          fd, true, base::MessagePumpLibevent::WATCH_READ, controller, watcher);
      evdev_watchers_.push_back(watcher);
      fd_controllers_.push_back(controller);
    } else {
      close(fd);
    }
  }
}

}  // namespace ui
