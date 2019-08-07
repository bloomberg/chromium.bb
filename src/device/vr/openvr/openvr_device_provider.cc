// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device_provider.h"

#include "base/metrics/histogram_macros.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/vr/isolated_gamepad_data_fetcher.h"
#include "device/vr/openvr/openvr_api_wrapper.h"
#include "device/vr/openvr/openvr_device.h"
#include "device/vr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

void OpenVRDeviceProvider::RecordRuntimeAvailability() {
  XrRuntimeAvailable runtime = XrRuntimeAvailable::NONE;
  if (vr::VR_IsRuntimeInstalled())
    runtime = XrRuntimeAvailable::OPENVR;
  UMA_HISTOGRAM_ENUMERATION("XR.RuntimeAvailable", runtime);
}

OpenVRDeviceProvider::OpenVRDeviceProvider() = default;

OpenVRDeviceProvider::~OpenVRDeviceProvider() {
  device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device::GAMEPAD_SOURCE_OPENVR);

  if (device_) {
    device_->Shutdown();
    device_ = nullptr;
  }

  OpenVRWrapper::SetTestHook(nullptr);
}

void OpenVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(device::mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojom::XRRuntimePtr)> add_device_callback,
    base::RepeatingCallback<void(device::mojom::XRDeviceId)>
        remove_device_callback,
    base::OnceClosure initialization_complete) {
  CreateDevice();
  if (device_) {
    add_device_callback.Run(device_->GetId(), device_->GetVRDisplayInfo(),
                            device_->BindXRRuntimePtr());
  }
  initialized_ = true;
  std::move(initialization_complete).Run();
}

void OpenVRDeviceProvider::SetTestHook(VRTestHook* test_hook) {
  OpenVRWrapper::SetTestHook(test_hook);
}

void OpenVRDeviceProvider::CreateDevice() {
  if (!vr::VR_IsRuntimeInstalled() || !vr::VR_IsHmdPresent())
    return;

  device_ = std::make_unique<OpenVRDevice>();
  if (device_->IsAvailable()) {
    GamepadDataFetcherManager::GetInstance()->AddFactory(
        new IsolatedGamepadDataFetcher::Factory(
            device::mojom::XRDeviceId::OPENVR_DEVICE_ID,
            device_->BindGamepadFactory()));
  } else {
    device_ = nullptr;
  }
}

bool OpenVRDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
