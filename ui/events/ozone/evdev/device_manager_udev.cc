// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/device_manager_udev.h"

#include <libudev.h>

#include "base/debug/trace_event.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/device_manager_evdev.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/scoped_udev.h"

namespace ui {

namespace {

const char kSubsystemInput[] = "input";

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
  if (priority <= SYS_LOG_ERR)
    LOG(ERROR) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
  else if (priority <= SYS_LOG_INFO)
    VLOG(1) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
  else  // SYS_LOG_DEBUG
    VLOG(2) << "libudev: " << fn << ": " << base::StringPrintV(format, args);
}

// Create libudev context.
scoped_udev UdevCreate() {
  struct udev* udev = udev_new();
  if (udev) {
    udev_set_log_fn(udev, UdevLog);
    udev_set_log_priority(udev, SYS_LOG_DEBUG);
  }
  return scoped_udev(udev);
}

// Start monitoring input device changes.
scoped_udev_monitor UdevCreateMonitor(struct udev* udev) {
  struct udev_monitor* monitor = udev_monitor_new_from_netlink(udev, "udev");
  if (monitor) {
    udev_monitor_filter_add_match_subsystem_devtype(
        monitor, kSubsystemInput, NULL);

    if (udev_monitor_enable_receiving(monitor))
      LOG(ERROR) << "failed to start receiving events from udev";
  }

  return scoped_udev_monitor(monitor);
}

// Enumerate all input devices using udev. Calls device_callback per device.
bool UdevEnumerateInputDevices(struct udev* udev,
                               const EvdevDeviceCallback& device_callback) {
  scoped_udev_enumerate enumerate(udev_enumerate_new(udev));
  if (!enumerate)
    return false;

  // Build list of devices with subsystem "input".
  udev_enumerate_add_match_subsystem(enumerate.get(), kSubsystemInput);
  udev_enumerate_scan_devices(enumerate.get());

  struct udev_list_entry* devices =
      udev_enumerate_get_list_entry(enumerate.get());
  struct udev_list_entry* entry;

  // Run callback per device in the list.
  udev_list_entry_foreach(entry, devices) {
    const char* name = udev_list_entry_get_name(entry);

    scoped_udev_device device(udev_device_new_from_syspath(udev, name));
    if (!device)
      continue;

    const char* path = udev_device_get_devnode(device.get());
    if (!path)
      continue;

    // Filter non-evdev device notes.
    if (!StartsWithASCII(path, "/dev/input/event", true))
      continue;

    // Found input device node; attach.
    device_callback.Run(base::FilePath(path));
  }

  return true;
}

// Device enumerator & monitor using udev.
//
// This class enumerates input devices attached to the system using udev.
//
// It also creates & monitors a udev netlink socket and issues callbacks for
// any changes to the set of attached devices.
//
// TODO(spang): Figure out how to test this.
class DeviceManagerUdev : public DeviceManagerEvdev,
                          base::MessagePumpLibevent::Watcher {
 public:
  DeviceManagerUdev() {}
  virtual ~DeviceManagerUdev() { Stop(); }

  // Enumerate existing devices & start watching for device changes.
  virtual void ScanAndStartMonitoring(const EvdevDeviceCallback& device_added,
                                      const EvdevDeviceCallback& device_removed)
      OVERRIDE {
    udev_ = UdevCreate();
    if (!udev_) {
      LOG(ERROR) << "failed to initialize libudev";
      return;
    }

    if (!StartMonitoring(device_added, device_removed))
      LOG(ERROR) << "failed to start monitoring device changes via udev";

    if (!UdevEnumerateInputDevices(udev_.get(), device_added))
      LOG(ERROR) << "failed to enumerate input devices via udev";
  }

  virtual void Stop() OVERRIDE {
    controller_.StopWatchingFileDescriptor();
    device_added_.Reset();
    device_removed_.Reset();
  }

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    // The netlink socket should never become disconnected. There's no need
    // to handle broken connections here.
    TRACE_EVENT1("ozone", "UdevDeviceChange", "socket", fd);

    scoped_udev_device device(udev_monitor_receive_device(udev_monitor_.get()));
    if (!device)
      return;

    const char* path = udev_device_get_devnode(device.get());
    const char* action = udev_device_get_action(device.get());
    if (!path || !action)
      return;

    if (!strcmp(action, "add") || !strcmp(action, "change"))
      device_added_.Run(base::FilePath(path));
    else if (!strcmp(action, "remove"))
      device_removed_.Run(base::FilePath(path));
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE { NOTREACHED(); }

 private:
  bool StartMonitoring(const EvdevDeviceCallback& device_added,
                       const EvdevDeviceCallback& device_removed) {
    udev_monitor_ = UdevCreateMonitor(udev_.get());
    if (!udev_monitor_)
      return false;

    // Grab monitor socket.
    int fd = udev_monitor_get_fd(udev_monitor_.get());
    if (fd < 0)
      return false;

    // Save callbacks.
    device_added_ = device_added;
    device_removed_ = device_removed;

    // Watch for incoming events on monitor socket.
    return base::MessageLoopForUI::current()->WatchFileDescriptor(
        fd, true, base::MessagePumpLibevent::WATCH_READ, &controller_, this);
  }

  // Udev daemon connection.
  scoped_udev udev_;

  // Udev device change monitor.
  scoped_udev_monitor udev_monitor_;

  // Callbacks for device changes.
  EvdevDeviceCallback device_added_;
  EvdevDeviceCallback device_removed_;

  // Watcher for uevent netlink socket.
  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagerUdev);
};

}  // namespace

scoped_ptr<DeviceManagerEvdev> CreateDeviceManagerUdev() {
  return make_scoped_ptr<DeviceManagerEvdev>(new DeviceManagerUdev());
}

}  // namespace ui
