// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory_evdev.h"

#include <fcntl.h>
#include <linux/input.h>

#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/device_manager_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/key_event_converter_evdev.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"

#if defined(USE_UDEV)
#include "ui/events/ozone/evdev/device_manager_udev.h"
#endif

#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID  _IOW('E', 0xa0, int)
#endif

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

// Open an input device. Opening may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on the FILE thread.
//
// This takes a TaskRunner and runs the reply on that thread, so that we
// can hop threads if necessary (back to the UI thread).
void OpenInputDevice(
    const base::FilePath& path,
    EventModifiersEvdev* modifiers,
    CursorDelegateEvdev* cursor,
    scoped_refptr<base::TaskRunner> reply_runner,
    base::Callback<void(scoped_ptr<EventConverterEvdev>)> reply_callback) {
  TRACE_EVENT1("ozone", "OpenInputDevice", "path", path.value());

  int fd = open(path.value().c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    PLOG(ERROR) << "Cannot open '" << path.value();
    return;
  }

  // Use monotonic timestamps for events. The touch code in particular
  // expects event timestamps to correlate to the monotonic clock
  // (base::TimeTicks).
  unsigned int clk = CLOCK_MONOTONIC;
  if (ioctl(fd, EVIOCSCLOCKID, &clk))
    PLOG(ERROR) << "failed to set CLOCK_MONOTONIC";

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

  // TODO(spang) Add more device types.
  scoped_ptr<EventConverterEvdev> converter;
  if (IsTouchScreen(devinfo))
    converter.reset(new TouchEventConverterEvdev(fd, path, devinfo));
  else if (devinfo.HasEventType(EV_KEY))
    converter.reset(new KeyEventConverterEvdev(fd, path, modifiers));

  if (converter) {
    // Reply with the constructed converter.
    reply_runner->PostTask(
        FROM_HERE, base::Bind(reply_callback, base::Passed(&converter)));
  } else {
    close(fd);
  }
}

// Close an input device. Closing may put the calling thread to sleep, and
// therefore should be run on a thread where latency is not critical. We
// run it on the FILE thread.
void CloseInputDevice(const base::FilePath& path,
                      scoped_ptr<EventConverterEvdev> converter) {
  TRACE_EVENT1("ozone", "CloseInputDevice", "path", path.value());
  converter.reset();
}

}  // namespace

EventFactoryEvdev::EventFactoryEvdev()
    : ui_task_runner_(base::MessageLoopProxy::current()),
      file_task_runner_(base::MessageLoopProxy::current()),
      cursor_(NULL),
      weak_ptr_factory_(this) {}

EventFactoryEvdev::EventFactoryEvdev(CursorDelegateEvdev* cursor)
    : ui_task_runner_(base::MessageLoopProxy::current()),
      file_task_runner_(base::MessageLoopProxy::current()),
      cursor_(cursor),
      weak_ptr_factory_(this) {}

EventFactoryEvdev::~EventFactoryEvdev() { STLDeleteValues(&converters_); }

void EventFactoryEvdev::AttachInputDevice(
    const base::FilePath& path,
    scoped_ptr<EventConverterEvdev> converter) {
  TRACE_EVENT1("ozone", "AttachInputDevice", "path", path.value());
  CHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // If we have an existing device, detach it. We don't want two
  // devices with the same name open at the same time.
  if (converters_[path])
    DetachInputDevice(path);

  // Add initialized device to map.
  converters_[path] = converter.release();
  converters_[path]->SetDispatchCallback(
      base::Bind(base::IgnoreResult(&EventFactoryEvdev::DispatchEvent),
                 base::Unretained(this)));
  converters_[path]->Start();
}

void EventFactoryEvdev::OnDeviceAdded(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "OnDeviceAdded", "path", path.value());

  // Dispatch task to open on FILE thread, since open may block.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&OpenInputDevice,
                 path,
                 &modifiers_,
                 cursor_,
                 ui_task_runner_,
                 base::Bind(&EventFactoryEvdev::AttachInputDevice,
                            weak_ptr_factory_.GetWeakPtr(),
                            path)));
}

void EventFactoryEvdev::DetachInputDevice(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "DetachInputDevice", "path", path.value());
  CHECK(ui_task_runner_->RunsTasksOnCurrentThread());

  // Remove device from map.
  scoped_ptr<EventConverterEvdev> converter(converters_[path]);
  converters_.erase(path);

  if (converter) {
    // Cancel libevent notifications from this converter. This part must be
    // on UI since the polling happens on UI.
    converter->Stop();

    // Dispatch task to close on FILE thread, since close may block.
    file_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CloseInputDevice, path, base::Passed(&converter)));
  }
}

void EventFactoryEvdev::OnDeviceRemoved(const base::FilePath& path) {
  TRACE_EVENT1("ozone", "OnDeviceRemoved", "path", path.value());
  DetachInputDevice(path);
}

void EventFactoryEvdev::StartProcessingEvents() {
  CHECK(ui_task_runner_->RunsTasksOnCurrentThread());

#if defined(USE_UDEV)
  // Scan for input devices using udev.
  device_manager_ = CreateDeviceManagerUdev();
#else
  // No udev support. Scan devices manually in /dev/input.
  device_manager_ = CreateDeviceManagerManual();
#endif

  // Scan & monitor devices.
  device_manager_->ScanAndStartMonitoring(
      base::Bind(&EventFactoryEvdev::OnDeviceAdded, base::Unretained(this)),
      base::Bind(&EventFactoryEvdev::OnDeviceRemoved, base::Unretained(this)));
}

void EventFactoryEvdev::SetFileTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {
  file_task_runner_ = task_runner;
}

void EventFactoryEvdev::WarpCursorTo(gfx::AcceleratedWidget widget,
                                     const gfx::PointF& location) {
  if (cursor_) {
    cursor_->MoveCursorTo(widget, location);
    MouseEvent mouse_event(ET_MOUSE_MOVED,
                           cursor_->location(),
                           cursor_->location(),
                           modifiers_.GetModifierFlags(),
                           /* changed_button_flags */ 0);
    DispatchEvent(&mouse_event);
  }
}

}  // namespace ui
