// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/event_factory_ozone.h"

#include "base/command_line.h"
#include "base/message_loop/message_pump_ozone.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event_switches.h"

namespace ui {

// static
EventFactoryOzone* EventFactoryOzone::impl_ = NULL;

EventFactoryOzone::EventFactoryOzone() {}

EventFactoryOzone::~EventFactoryOzone() {
  // Always delete watchers before converters to prevent a possible race.
  STLDeleteValues<std::map<int, FDWatcher> >(&watchers_);
  STLDeleteValues<std::map<int, Converter> >(&converters_);
}

EventFactoryOzone* EventFactoryOzone::GetInstance() {
  CHECK(impl_) << "No EventFactoryOzone implementation set.";
  return impl_;
}

void EventFactoryOzone::SetInstance(EventFactoryOzone* impl) { impl_ = impl; }

void EventFactoryOzone::StartProcessingEvents() {}

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
