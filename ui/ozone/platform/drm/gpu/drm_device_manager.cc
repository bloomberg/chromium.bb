// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"

#include "base/file_descriptor_posix.h"
#include "base/single_thread_task_runner.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"

namespace ui {

namespace {

class FindByDevicePath {
 public:
  explicit FindByDevicePath(const base::FilePath& path) : path_(path) {}

  bool operator()(const scoped_refptr<DrmDevice>& device) {
    return device->device_path() == path_;
  }

 private:
  base::FilePath path_;
};

}  // namespace

DrmDeviceManager::DrmDeviceManager(
    scoped_ptr<DrmDeviceGenerator> drm_device_generator)
    : drm_device_generator_(drm_device_generator.Pass()) {
}

DrmDeviceManager::~DrmDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(drm_device_map_.empty());
}

bool DrmDeviceManager::AddDrmDevice(const base::FilePath& path,
                                    const base::FileDescriptor& fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::File file(fd.fd);
  auto it =
      std::find_if(devices_.begin(), devices_.end(), FindByDevicePath(path));
  if (it != devices_.end()) {
    VLOG(2) << "Got request to add existing device: " << path.value();
    return false;
  }

  scoped_refptr<DrmDevice> device =
      drm_device_generator_->CreateDevice(path, file.Pass());
  if (!device) {
    LOG(ERROR) << "Could not initialize DRM device for " << path.value();
    return false;
  }

  if (io_task_runner_)
    device->InitializeTaskRunner(io_task_runner_);

  if (!primary_device_)
    primary_device_ = device;

  devices_.push_back(device);
  return true;
}

void DrmDeviceManager::RemoveDrmDevice(const base::FilePath& path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it =
      std::find_if(devices_.begin(), devices_.end(), FindByDevicePath(path));
  if (it == devices_.end()) {
    VLOG(2) << "Got request to remove non-existent device: " << path.value();
    return;
  }

  DCHECK_NE(primary_device_, *it);
  devices_.erase(it);
}

void DrmDeviceManager::InitializeIOTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_task_runner_);
  io_task_runner_ = task_runner;
  for (const auto& device : devices_)
    device->InitializeTaskRunner(io_task_runner_);
}

void DrmDeviceManager::UpdateDrmDevice(gfx::AcceleratedWidget widget,
                                       const scoped_refptr<DrmDevice>& device) {
  base::AutoLock lock(lock_);
  drm_device_map_[widget] = device;
}

void DrmDeviceManager::RemoveDrmDevice(gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  auto it = drm_device_map_.find(widget);
  if (it != drm_device_map_.end())
    drm_device_map_.erase(it);
}

scoped_refptr<DrmDevice> DrmDeviceManager::GetDrmDevice(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  if (widget == gfx::kNullAcceleratedWidget)
    return primary_device_;

  auto it = drm_device_map_.find(widget);
  DCHECK(it != drm_device_map_.end())
      << "Attempting to get device for unknown widget " << widget;
  // If the widget isn't associated with a display (headless mode) we can
  // allocate buffers from any controller since they will never be scanned out.
  // Use the primary DRM device as a fallback when allocating these buffers.
  if (!it->second)
    return primary_device_;

  return it->second;
}

const DrmDeviceVector& DrmDeviceManager::GetDrmDevices() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return devices_;
}

}  // namespace ui
