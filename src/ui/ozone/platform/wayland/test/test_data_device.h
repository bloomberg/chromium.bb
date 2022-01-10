// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_

#include <wayland-server-protocol.h>

#include <cstdint>

#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_selection_device_manager.h"

struct wl_client;
struct wl_resource;

namespace wl {

extern const struct wl_data_device_interface kTestDataDeviceImpl;

class TestDataOffer;
class TestDataSource;

class TestDataDevice : public TestSelectionDevice {
 public:
  struct DragDelegate {
    virtual void StartDrag(TestDataSource* source,
                           MockSurface* origin,
                           uint32_t serial) = 0;
  };

  TestDataDevice(wl_resource* resource, wl_client* client);

  TestDataDevice(const TestDataDevice&) = delete;
  TestDataDevice& operator=(const TestDataDevice&) = delete;

  ~TestDataDevice() override;

  void set_drag_delegate(DragDelegate* delegate) { drag_delegate_ = delegate; }

  TestDataOffer* CreateAndSendDataOffer();
  void SetSelection(TestDataSource* data_source, uint32_t serial);
  void StartDrag(TestDataSource* data_source,
                 MockSurface* origin,
                 uint32_t serial);

  void OnEnter(uint32_t serial,
               wl_resource* surface,
               wl_fixed_t x,
               wl_fixed_t y,
               TestDataOffer* data_offer);
  void OnLeave();
  void OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void OnDrop();

  wl_client* client() { return client_; }

 private:
  wl_client* client_ = nullptr;
  DragDelegate* drag_delegate_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_DATA_DEVICE_H_
