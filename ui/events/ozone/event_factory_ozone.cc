// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/event_factory_ozone.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include "base/message_loop/message_pump_ozone.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/key_event_converter_ozone.h"
#include "ui/events/ozone/evdev/touch_event_converter_ozone.h"
#include "ui/events/ozone/event_factory_delegate_ozone.h"

namespace ui {

// static
EventFactoryDelegateOzone* EventFactoryOzone::delegate_ = NULL;

EventFactoryOzone::EventFactoryOzone() {}

EventFactoryOzone::~EventFactoryOzone() {
  // Always delete watchers before converters to prevent a possible race.
  STLDeleteValues<std::map<int, FDWatcher> >(&watchers_);
  STLDeleteValues<std::map<int, Converter> >(&converters_);
}

void EventFactoryOzone::CreateStartupEventConverters() {
  if (delegate_) {
    delegate_->CreateStartupEventConverters(this);
    return;
  }

  // If there is no |delegate_| set, read events from /dev/input/*

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
      converter.reset(new TouchEventConverterOzone(fd, id));
    else if (evtype & (1 << EV_KEY))
      converter.reset(new KeyEventConverterOzone());

    if (converter) {
      AddEventConverter(fd, converter.Pass());
    } else {
      close(fd);
    }
  }
}

void EventFactoryOzone::SetEventFactoryDelegateOzone(
    EventFactoryDelegateOzone* delegate) {
  // It should be unnecessary to call this more than once.
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void EventFactoryOzone::AddEventConverter(
    int fd,
    scoped_ptr<EventConverterOzone> converter) {
  CHECK(watchers_.count(fd) == 0 && converters_.count(fd) == 0);

  FDWatcher watcher = new base::MessagePumpLibevent::FileDescriptorWatcher();

  base::MessagePumpOzone::Current()->WatchFileDescriptor(
      fd,
      true,
      base::MessagePumpLibevent::WATCH_READ,
      watcher,
      converter.get());

  converters_[fd] = converter.release();
  watchers_[fd] = watcher;
}

void EventFactoryOzone::RemoveEventConverter(int fd) {
  // Always delete watchers before converters to prevent a possible race.
  delete watchers_[fd];
  delete converters_[fd];
  watchers_.erase(fd);
  converters_.erase(fd);
}

}  // namespace ui
