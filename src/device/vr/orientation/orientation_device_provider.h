// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ORIENTATION_DEVICE_PROVIDER_H
#define DEVICE_VR_ORIENTATION_DEVICE_PROVIDER_H

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "device/vr/orientation/orientation_device.h"
#include "device/vr/public/cpp/vr_device_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class COMPONENT_EXPORT(VR_ORIENTATION) VROrientationDeviceProvider
    : public VRDeviceProvider {
 public:
  explicit VROrientationDeviceProvider(
      mojo::PendingRemote<device::mojom::SensorProvider> sensor_provider);
  ~VROrientationDeviceProvider() override;

  void Initialize(
      base::RepeatingCallback<void(mojom::XRDeviceId,
                                   mojom::VRDisplayInfoPtr,
                                   mojo::PendingRemote<mojom::XRRuntime>)>
          add_device_callback,
      base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
      base::OnceClosure initialization_complete) override;

  bool Initialized() override;

 private:
  void DeviceInitialized();

  bool initialized_ = false;

  mojo::Remote<device::mojom::SensorProvider> sensor_provider_;

  std::unique_ptr<VROrientationDevice> device_;

  base::RepeatingCallback<void(mojom::XRDeviceId,
                               mojom::VRDisplayInfoPtr,
                               mojo::PendingRemote<mojom::XRRuntime>)>
      add_device_callback_;
  base::OnceClosure initialized_callback_;

  DISALLOW_COPY_AND_ASSIGN(VROrientationDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_ORIENTATION_DEVICE_PROVIDER_H
