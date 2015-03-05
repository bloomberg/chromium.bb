// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DISPLAY_CHANGE_OBSERVER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DISPLAY_CHANGE_OBSERVER_H_

namespace ui {

class HardwareDisplayController;

class DisplayChangeObserver {
 public:
  virtual ~DisplayChangeObserver() {}

  // Called when |controller| was changed/added.
  virtual void OnDisplayChanged(HardwareDisplayController* controller) = 0;

  // Called just before |controller| is removed. Observers that cached
  // |controller| must invalidate it at this point.
  virtual void OnDisplayRemoved(HardwareDisplayController* controller) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DISPLAY_CHANGE_OBSERVER_H_
