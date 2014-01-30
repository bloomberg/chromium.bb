// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#if defined(USE_UDEV)
#include <libudev.h>
#endif

#include "base/callback.h"
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

#if defined(USE_UDEV)

// Severity levels from syslog.h. We can't include it directly as it
// conflicts with base/logging.h
enum {
  SYS_LOG_EMERG = 0,
  SYS_LOG_ALERT = 1,
  SYS_LOG_CRIT = 2,
  SYS_LOG_ERR = 3,
  SYS_LOG_WARNING = 4,
  SYS_LOG_NOTICE = 5,
  SYS_LOG_INFO = 6,
  SYS_LOG_DEBUG = 7,
};

// Log handler for messages generated from libudev.
void UdevLog(struct udev* udev,
             int priority,
             const char* file,
             int line,
             const char* fn,
             const char* format,
             va_list args) {
  std::string message = base::StringPrintf("libudev: %s: ", fn);
  base::StringAppendV(&message, format, args);
  if (priority <= SYS_LOG_ERR)
    LOG(ERROR) << message;
  else if (priority <= SYS_LOG_INFO)
    VLOG(1) << message;
  else  // SYS_LOG_DEBUG
    VLOG(2) << message;
}

// Connect to the udev daemon.
scoped_udev UdevConnect() {
  struct udev* udev = udev_new();
  if (udev) {
    udev_set_log_fn(udev, UdevLog);
    udev_set_log_priority(udev, SYS_LOG_DEBUG);
  }
  return scoped_udev(udev);
}

// Enumerate all input devices using udev. Calls device_callback per device.
bool UdevEnumerateInputDevices(
    struct udev* udev,
    base::Callback<void(const base::FilePath&)> device_callback) {
  scoped_udev_enumerate enumerate(udev_enumerate_new(udev));
  if (!enumerate)
    return false;

  udev_enumerate_add_match_subsystem(enumerate.get(), "input");
  udev_enumerate_scan_devices(enumerate.get());

  struct udev_list_entry* devices =
      udev_enumerate_get_list_entry(enumerate.get());
  struct udev_list_entry* entry;

  udev_list_entry_foreach(entry, devices) {
    const char* name = udev_list_entry_get_name(entry);

    scoped_udev_device device(udev_device_new_from_syspath(udev, name));
    if (!device)
      continue;

    const char* path = udev_device_get_devnode(device.get());
    if (!path)
      continue;

    // Found input device node; attach.
    device_callback.Run(base::FilePath(path));
  }

  return true;
}

#endif  // defined(USE_UDEV)

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
  scoped_ptr<EventConverterEvdev> converter;
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

#if defined(USE_UDEV)
  // Scan for input devices using udev.
  StartProcessingEventsUdev();
#else
  // No udev support. Scan devices manually in /dev/input.
  StartProcessingEventsManual();
#endif
}

void EventFactoryEvdev::StartProcessingEventsManual() {
  base::FileEnumerator file_enum(base::FilePath("/dev/input"),
                                 false,
                                 base::FileEnumerator::FILES,
                                 "event*[0-9]");
  for (base::FilePath path = file_enum.Next(); !path.empty();
       path = file_enum.Next())
    AttachInputDevice(path);
}

#if defined(USE_UDEV)
void EventFactoryEvdev::StartProcessingEventsUdev() {
  udev_ = UdevConnect();
  if (!udev_) {
    LOG(ERROR) << "failed to connect to udev";
    return;
  }
  if (!UdevEnumerateInputDevices(
           udev_.get(),
           base::Bind(&EventFactoryEvdev::AttachInputDevice,
                      base::Unretained(this))))
    LOG(ERROR) << "failed to enumerate input devices via udev";
}
#endif

}  // namespace ui
