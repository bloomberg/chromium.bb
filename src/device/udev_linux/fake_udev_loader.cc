// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/udev_linux/fake_udev_loader.h"

#include <base/logging.h>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"

struct udev {
  // empty
};

struct udev_device {
  const std::string name_;
  const std::string syspath_;
  const std::string subsystem_;
  std::map<std::string, std::string> sysattrs_;
  std::map<std::string, std::string> properties_;

  udev_device(std::string name,
              std::string syspath,
              std::string subsystem,
              std::map<std::string, std::string> sysattrs,
              std::map<std::string, std::string> properties)
      : name_(std::move(name)),
        syspath_(std::move(syspath)),
        subsystem_(std::move(subsystem)),
        sysattrs_(std::move(sysattrs)),
        properties_(std::move(properties)) {}

  DISALLOW_COPY_AND_ASSIGN(udev_device);
};

struct udev_list_entry {
  explicit udev_list_entry(std::string name) : name_(std::move(name)) {}
  udev_list_entry(const udev_list_entry& other) = delete;
  udev_list_entry& operator=(const udev_list_entry& other) = delete;

  const std::string name_;
  udev_list_entry* next_ = nullptr;
};

struct udev_enumerate {
  explicit udev_enumerate(
      const std::vector<std::unique_ptr<udev_device>>& devices) {
    for (const auto& device : devices) {
      auto entry = std::make_unique<udev_list_entry>(device->syspath_);
      if (!entries_.empty()) {
        entries_.back()->next_ = entry.get();
      }
      entries_.push_back(std::move(entry));
    }
  }

  udev_enumerate(const udev_enumerate& other) = delete;
  udev_enumerate& operator=(const udev_enumerate& other) = delete;

  std::vector<std::unique_ptr<udev_list_entry>> entries_;
};

struct udev_monitor {
  udev_monitor() {
    bool res = base::CreatePipe(&read_fd_, &write_fd_, true);
    DCHECK(res);
  }

  udev_monitor(const udev_monitor& other) = delete;
  udev_monitor& operator=(const udev_monitor& other) = delete;

  // |read_fd_| will be returned by udev_monitor_get_fd() and will be signaled
  // by writing to |write_fd_| to indicate that an event is available.
  base::ScopedFD read_fd_;
  base::ScopedFD write_fd_;
};

namespace testing {

FakeUdevLoader::FakeUdevLoader() {
  // Nothing to construct, just register it as testing backend.
  UdevLoader::SetForTesting(this, true);
}

FakeUdevLoader::~FakeUdevLoader() {
  // Clean up after ourselves if this instance of fake udev loader was used
  // as test backend.
  if (UdevLoader::Get() == this)
    UdevLoader::SetForTesting(nullptr, false);
}

udev_device* FakeUdevLoader::AddFakeDevice(
    std::string name,
    std::string syspath,
    std::string subsystem,
    std::map<std::string, std::string> sysattrs,
    std::map<std::string, std::string> properties) {
  devices_.emplace_back(
      new udev_device(std::move(name), std::move(syspath), std::move(subsystem),
                      std::move(sysattrs), std::move(properties)));
  return devices_.back().get();
}

void FakeUdevLoader::Reset() {
  devices_.clear();
}

bool FakeUdevLoader::Init() {
  return true;
}

const char* FakeUdevLoader::udev_device_get_action(udev_device* udev_device) {
  return nullptr;
}

const char* FakeUdevLoader::udev_device_get_devnode(udev_device* udev_device) {
  return nullptr;
}

const char* FakeUdevLoader::udev_device_get_devtype(udev_device* udev_device) {
  return nullptr;
}

udev_device* FakeUdevLoader::udev_device_get_parent(udev_device* udev_device) {
  if (!udev_device) {
    return nullptr;
  }

  const base::FilePath syspath(udev_device->syspath_);
  auto it =
      std::find_if(devices_.begin(), devices_.end(), [syspath](const auto& d) {
        return base::FilePath(d->syspath_).IsParent(syspath);
      });
  return it == devices_.end() ? nullptr : it->get();
}

udev_device* FakeUdevLoader::udev_device_get_parent_with_subsystem_devtype(
    udev_device* udev_device,
    const char* subsystem,
    const char* devtype) {
  return nullptr;
}

const char* FakeUdevLoader::udev_device_get_property_value(
    udev_device* udev_device,
    const char* key) {
  const auto it = udev_device->properties_.find(key);
  return it == udev_device->properties_.end() ? nullptr : it->second.c_str();
}

const char* FakeUdevLoader::udev_device_get_subsystem(
    udev_device* udev_device) {
  return udev_device->subsystem_.c_str();
}

const char* FakeUdevLoader::udev_device_get_sysattr_value(
    udev_device* udev_device,
    const char* sysattr) {
  auto it = udev_device->sysattrs_.find(sysattr);
  return it == udev_device->sysattrs_.end() ? nullptr : it->second.c_str();
}

const char* FakeUdevLoader::udev_device_get_sysname(udev_device* udev_device) {
  return udev_device->name_.c_str();
}

const char* FakeUdevLoader::udev_device_get_syspath(udev_device* udev_device) {
  return udev_device->syspath_.c_str();
}

udev_device* FakeUdevLoader::udev_device_new_from_devnum(udev* udev,
                                                         char type,
                                                         dev_t devnum) {
  return nullptr;
}

udev_device* FakeUdevLoader::udev_device_new_from_subsystem_sysname(
    udev* udev,
    const char* subsystem,
    const char* sysname) {
  return nullptr;
}

udev_device* FakeUdevLoader::udev_device_new_from_syspath(udev* udev,
                                                          const char* syspath) {
  auto it =
      std::find_if(devices_.begin(), devices_.end(),
                   [syspath](const auto& d) { return d->syspath_ == syspath; });
  return it == devices_.end() ? nullptr : it->get();
}

void FakeUdevLoader::udev_device_unref(udev_device* udev_device) {
  // Nothing to do, the device will be destroyed when FakeUdevLoader instance
  // gets destroyed.
}

int FakeUdevLoader::udev_enumerate_add_match_subsystem(
    udev_enumerate* udev_enumerate,
    const char* subsystem) {
  return 0;
}

udev_list_entry* FakeUdevLoader::udev_enumerate_get_list_entry(
    udev_enumerate* udev_enumerate) {
  if (udev_enumerate->entries_.empty()) {
    return nullptr;
  }
  return udev_enumerate->entries_.front().get();
}

udev_enumerate* FakeUdevLoader::udev_enumerate_new(udev* udev) {
  return new udev_enumerate(devices_);
}

int FakeUdevLoader::udev_enumerate_scan_devices(
    udev_enumerate* udev_enumerate) {
  return 0;
}

void FakeUdevLoader::udev_enumerate_unref(udev_enumerate* udev_enumerate) {
  delete udev_enumerate;
}

udev_list_entry* FakeUdevLoader::udev_list_entry_get_next(
    udev_list_entry* list_entry) {
  return list_entry->next_;
}

const char* FakeUdevLoader::udev_list_entry_get_name(
    udev_list_entry* list_entry) {
  return list_entry->name_.c_str();
}

int FakeUdevLoader::udev_monitor_enable_receiving(udev_monitor* udev_monitor) {
  return 0;
}

int FakeUdevLoader::udev_monitor_filter_add_match_subsystem_devtype(
    udev_monitor* udev_monitor,
    const char* subsystem,
    const char* devtype) {
  return 0;
}

int FakeUdevLoader::udev_monitor_get_fd(udev_monitor* udev_monitor) {
  return udev_monitor->read_fd_.get();
}

udev_monitor* FakeUdevLoader::udev_monitor_new_from_netlink(udev* udev,
                                                            const char* name) {
  return new udev_monitor;
}

udev_device* FakeUdevLoader::udev_monitor_receive_device(
    udev_monitor* udev_monitor) {
  return nullptr;
}

void FakeUdevLoader::udev_monitor_unref(udev_monitor* udev_monitor) {
  delete udev_monitor;
}

udev* FakeUdevLoader::udev_new() {
  return new udev;
}

void FakeUdevLoader::udev_unref(udev* udev) {
  delete udev;
}

void FakeUdevLoader::udev_set_log_fn(struct udev* udev,
                                     void (*log_fn)(struct udev* udev,
                                                    int priority,
                                                    const char* file,
                                                    int line,
                                                    const char* fn,
                                                    const char* format,
                                                    va_list args)) {}

void FakeUdevLoader::udev_set_log_priority(struct udev* udev, int priority) {}

}  // namespace testing
