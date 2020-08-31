// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/service/isolated_device_provider.h"

#include "base/bind.h"
#include "content/browser/xr/service/xr_device_service.h"
#include "content/browser/xr/xr_utils.h"
#include "content/public/browser/xr_integration_client.h"

namespace {
constexpr int kMaxRetries = 3;
}

namespace content {

void IsolatedVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                 device::mojom::VRDisplayInfoPtr,
                                 mojo::PendingRemote<device::mojom::XRRuntime>)>
        add_device_callback,
    base::RepeatingCallback<void(device::mojom::XRDeviceId)>
        remove_device_callback,
    base::OnceClosure initialization_complete) {
  add_device_callback_ = std::move(add_device_callback);
  remove_device_callback_ = std::move(remove_device_callback);
  initialization_complete_ = std::move(initialization_complete);

  SetupDeviceProvider();
}

bool IsolatedVRDeviceProvider::Initialized() {
  return initialized_;
}

void IsolatedVRDeviceProvider::OnDeviceAdded(
    mojo::PendingRemote<device::mojom::XRRuntime> device,
    mojo::PendingRemote<device::mojom::XRCompositorHost> compositor_host,
    device::mojom::XRDeviceId device_id) {
  add_device_callback_.Run(device_id, nullptr, std::move(device));

  auto* integration_client = GetXrIntegrationClient();
  if (!integration_client)
    return;

  // It's perfectly valid to insert nullptr, and doing so avoids the extra move
  // if we were to do an assignment/check to avoid inserting it.
  ui_host_map_.insert(
      std::make_pair(device_id, integration_client->CreateVrUiHost(
                                    device_id, std::move(compositor_host))));
}

void IsolatedVRDeviceProvider::OnDeviceRemoved(device::mojom::XRDeviceId id) {
  remove_device_callback_.Run(id);
  ui_host_map_.erase(id);
}

void IsolatedVRDeviceProvider::OnServerError() {
  // An error occurred - any devices we have added are now disconnected and
  // should be removed.
  for (auto& entry : ui_host_map_) {
    auto id = entry.first;
    remove_device_callback_.Run(id);
  }
  ui_host_map_.clear();

  // At this point, XRRuntimeManagerImpl may be blocked waiting for us to return
  // that we've enumerated all runtimes/devices.  If we lost the connection to
  // the service, we'll try again.  If we've already tried too many times,
  // then just assume we won't ever get devices, so report we are done now.
  // This will unblock WebXR/WebVR promises so they can reject indicating we
  // never found devices.
  if (!initialized_ && retry_count_ >= kMaxRetries) {
    OnDevicesEnumerated();
  } else {
    device_provider_.reset();
    receiver_.reset();
    retry_count_++;
    SetupDeviceProvider();
  }
}

void IsolatedVRDeviceProvider::OnDevicesEnumerated() {
  if (!initialized_) {
    initialized_ = true;
    std::move(initialization_complete_).Run();
  }

  // Either we've hit the max retries and given up (in which case we don't have
  // a device provider which could error out and cause us to retry) or we've
  // successfully gotten the device provider again after a retry, and we should
  // reset our count in case it gets disconnected.
  retry_count_ = 0;
}

void IsolatedVRDeviceProvider::SetupDeviceProvider() {
  GetXRDeviceService()->BindRuntimeProvider(
      device_provider_.BindNewPipeAndPassReceiver());
  device_provider_.set_disconnect_handler(base::BindOnce(
      &IsolatedVRDeviceProvider::OnServerError, base::Unretained(this)));

  device_provider_->RequestDevices(receiver_.BindNewPipeAndPassRemote());
}

IsolatedVRDeviceProvider::IsolatedVRDeviceProvider() = default;

// Default destructor handles renderer_host_map_ cleanup.
IsolatedVRDeviceProvider::~IsolatedVRDeviceProvider() = default;

}  // namespace content
