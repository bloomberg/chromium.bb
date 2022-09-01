// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/arcore_device_provider.h"

#include "components/webxr/android/arcore_java_utils.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_device.h"
#include "device/vr/android/arcore/arcore_impl.h"
#include "device/vr/android/arcore/arcore_shim.h"

namespace webxr {

ArCoreDeviceProvider::ArCoreDeviceProvider(
    webxr::ArCompositorDelegateProvider compositor_delegate_provider)
    : compositor_delegate_provider_(compositor_delegate_provider) {}

ArCoreDeviceProvider::~ArCoreDeviceProvider() = default;

void ArCoreDeviceProvider::Initialize(device::VRDeviceProviderClient* client) {
  if (device::IsArCoreSupported()) {
    DVLOG(2) << __func__ << ": ARCore is supported, creating device";

    arcore_device_ = std::make_unique<device::ArCoreDevice>(
        std::make_unique<device::ArCoreImplFactory>(),
        std::make_unique<device::ArImageTransportFactory>(),
        std::make_unique<webxr::MailboxToSurfaceBridgeFactoryImpl>(),
        std::make_unique<webxr::ArCoreJavaUtils>(compositor_delegate_provider_),
        client->GetXrFrameSinkClientFactory());

    client->AddRuntime(
        arcore_device_->GetId(), arcore_device_->GetVRDisplayInfo(),
        arcore_device_->GetDeviceData(), arcore_device_->BindXRRuntime());
  }
  initialized_ = true;
  client->OnProviderInitialized();
}

bool ArCoreDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace webxr
