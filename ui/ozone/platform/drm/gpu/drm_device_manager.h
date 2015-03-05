// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

class DrmDevice;

// Tracks the mapping between widgets and the DRM devices used to allocate
// buffers for the window represented by the widget.
class OZONE_EXPORT DrmDeviceManager {
 public:
  DrmDeviceManager(const scoped_refptr<DrmDevice>& primary_device);
  ~DrmDeviceManager();

  // Updates the device associated with |widget|.
  void UpdateDrmDevice(gfx::AcceleratedWidget widget,
                       const scoped_refptr<DrmDevice>& device);

  // Removes the device associated with |widget|.
  void RemoveDrmDevice(gfx::AcceleratedWidget widget);

  // Returns the device associated with |widget|. If there is no association
  // returns |primary_device_|.
  scoped_refptr<DrmDevice> GetDrmDevice(gfx::AcceleratedWidget widget);

 private:
  std::map<gfx::AcceleratedWidget, scoped_refptr<DrmDevice>> drm_device_map_;

  // This device represents the primary graphics device and is used when:
  // 1) 'widget == kNullAcceleratedWidget' when the API requesting a buffer has
  // no knowledge of the surface/display it belongs to (currently this happens
  // for video buffers), or
  // 2) in order to allocate buffers for unmatched surfaces (surfaces without a
  // display; ie: when in headless mode).
  scoped_refptr<DrmDevice> primary_device_;

  // This class is accessed from the main thread and the IO thread. This lock
  // protects access to the device map.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(DrmDeviceManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DEVICE_MANAGER_H_
