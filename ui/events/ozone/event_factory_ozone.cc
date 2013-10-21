// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/event_factory_ozone.h"

#include "base/command_line.h"
#include "base/message_loop/message_pump_ozone.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event_switches.h"
#include "ui/events/ozone/event_factory_delegate_ozone.h"

#if defined(USE_OZONE_EVDEV)
#include "ui/events/ozone/evdev/event_factory_delegate.h"
#endif

namespace ui {

namespace {

#if defined(USE_OZONE_EVDEV)
static const char kEventFactoryEvdev[] = "evdev";
#endif

EventFactoryDelegateOzone* CreateDelegate(const std::string& event_delegate) {
#if defined(USE_OZONE_EVDEV)
  if (event_delegate == "evdev" || event_delegate == "default")
    return new EventFactoryDelegateEvdev;
#endif

  if (event_delegate == "none" || event_delegate == "default") {
    LOG(WARNING) << "No ozone events implementation - limited input support";
    return NULL;
  }

  LOG(FATAL) << "Invalid ozone events implementation: " << event_delegate;
  return NULL;  // not reached
}

std::string GetRequestedDelegate() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kOzoneEvents))
    return "default";
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kOzoneEvents);
}

}  // namespace

// static
EventFactoryDelegateOzone* EventFactoryOzone::delegate_ = NULL;

EventFactoryOzone::EventFactoryOzone() {}

EventFactoryOzone::~EventFactoryOzone() {
  // Always delete watchers before converters to prevent a possible race.
  STLDeleteValues<std::map<int, FDWatcher> >(&watchers_);
  STLDeleteValues<std::map<int, Converter> >(&converters_);
}

void EventFactoryOzone::CreateStartupEventConverters() {
  if (!delegate_) {
    std::string requested_delegate = GetRequestedDelegate();
    SetEventFactoryDelegateOzone(CreateDelegate(requested_delegate));
  }
  if (delegate_)
    delegate_->CreateStartupEventConverters(this);
}

// static
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
