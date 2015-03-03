// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/drm_device_manager.h"

#include "ui/ozone/platform/dri/drm_device.h"

namespace ui {

DrmDeviceManager::DrmDeviceManager(
    const scoped_refptr<DrmDevice>& primary_device)
    : primary_device_(primary_device) {
}

DrmDeviceManager::~DrmDeviceManager() {
  DCHECK(drm_device_map_.empty());
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

}  // namespace ui
