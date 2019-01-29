// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_MANAGER_H_

#include <wayland-server-protocol-core.h>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct wl_data_device_manager_interface kTestDataDeviceManagerImpl;

class TestDataDevice;
class TestDataSource;

// Manage wl_data_device_manager object.
class TestDataDeviceManager : public GlobalObject {
 public:
  TestDataDeviceManager();
  ~TestDataDeviceManager() override;

  TestDataDevice* data_device() const { return data_device_; }
  void set_data_device(TestDataDevice* data_device) {
    data_device_ = data_device;
  }

  TestDataSource* data_source() const { return data_source_; }
  void set_data_source(TestDataSource* data_source) {
    data_source_ = data_source;
  }

 private:
  TestDataDevice* data_device_;
  TestDataSource* data_source_;

  DISALLOW_COPY_AND_ASSIGN(TestDataDeviceManager);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_MANAGER_H_
