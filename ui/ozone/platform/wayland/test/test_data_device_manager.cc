// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"

#include <wayland-server-core.h>

#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"

namespace wl {

namespace {

constexpr uint32_t kDataDeviceManagerVersion = 3;

void CreateDataSource(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* data_source_resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  if (!data_source_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  SetImplementation(data_source_resource, &kTestDataSourceImpl,
                    std::make_unique<TestDataSource>(data_source_resource));

  auto* data_device_manager = GetUserDataAs<TestDataDeviceManager>(resource);
  data_device_manager->set_data_source(
      GetUserDataAs<TestDataSource>(data_source_resource));
}

void GetDataDevice(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* seat_resource) {
  wl_resource* data_device_resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  if (!data_device_resource) {
    wl_client_post_no_memory(client);
    return;
  }

  SetImplementation(
      data_device_resource, &kTestDataDeviceImpl,
      std::make_unique<TestDataDevice>(client, data_device_resource));

  auto* data_device_manager = GetUserDataAs<TestDataDeviceManager>(resource);
  data_device_manager->set_data_device(
      GetUserDataAs<TestDataDevice>(data_device_resource));
}

}  // namespace

const struct wl_data_device_manager_interface kTestDataDeviceManagerImpl = {
    &CreateDataSource, &GetDataDevice};

TestDataDeviceManager::TestDataDeviceManager()
    : GlobalObject(&wl_data_device_manager_interface,
                   &kTestDataDeviceManagerImpl,
                   kDataDeviceManagerVersion) {}

TestDataDeviceManager::~TestDataDeviceManager() {}

}  // namespace wl
