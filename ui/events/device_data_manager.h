// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICE_DATA_MANAGER_H_
#define UI_EVENTS_DEVICE_DATA_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/events_base_export.h"
#include "ui/gfx/transform.h"

namespace ui {

// Keeps track of device mappings and event transformations.
class EVENTS_BASE_EXPORT DeviceDataManager {
 public:
  virtual ~DeviceDataManager();

  static void CreateInstance();
  static DeviceDataManager* GetInstance();
  static bool HasInstance();

  void ClearTouchTransformerRecord();
  void UpdateTouchInfoForDisplay(int64_t display_id,
                                 int touch_device_id,
                                 const gfx::Transform& touch_transformer);
  void ApplyTouchTransformer(int touch_device_id, float* x, float* y);
  int64_t GetDisplayForTouchDevice(int touch_device_id) const;

 protected:
  DeviceDataManager();

  static DeviceDataManager* instance();

  static const int kMaxDeviceNum = 128;

 private:
  static DeviceDataManager* instance_;

  bool IsTouchDeviceIdValid(int touch_device_id) const;

  // Table to keep track of which display id is mapped to which touch device.
  int64_t touch_device_to_display_map_[kMaxDeviceNum];
  // Index table to find the TouchTransformer for a touch device.
  gfx::Transform touch_device_transformer_map_[kMaxDeviceNum];

  DISALLOW_COPY_AND_ASSIGN(DeviceDataManager);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICE_DATA_MANAGER_H_
