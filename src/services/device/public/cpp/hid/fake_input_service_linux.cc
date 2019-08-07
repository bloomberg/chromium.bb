// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/fake_input_service_linux.h"

namespace device {

FakeInputServiceLinux::FakeInputServiceLinux() {}

FakeInputServiceLinux::~FakeInputServiceLinux() {}

// mojom::InputDeviceManager implementation:
void FakeInputServiceLinux::GetDevicesAndSetClient(
    mojom::InputDeviceManagerClientAssociatedPtrInfo client,
    GetDevicesCallback callback) {
  GetDevices(std::move(callback));

  if (!client.is_valid())
    return;

  mojom::InputDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void FakeInputServiceLinux::GetDevices(GetDevicesCallback callback) {
  std::vector<mojom::InputDeviceInfoPtr> devices;
  for (auto& device : devices_)
    devices.push_back(device.second->Clone());

  std::move(callback).Run(std::move(devices));
}

void FakeInputServiceLinux::Bind(mojom::InputDeviceManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void FakeInputServiceLinux::AddDevice(mojom::InputDeviceInfoPtr info) {
  auto* device_info = info.get();
  clients_.ForAllPtrs([device_info](mojom::InputDeviceManagerClient* client) {
    client->InputDeviceAdded(device_info->Clone());
  });

  devices_[info->id] = std::move(info);
}

void FakeInputServiceLinux::RemoveDevice(const std::string& id) {
  devices_.erase(id);

  clients_.ForAllPtrs([id](mojom::InputDeviceManagerClient* client) {
    client->InputDeviceRemoved(id);
  });
}

}  // namespace device
