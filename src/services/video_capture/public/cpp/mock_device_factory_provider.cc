// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_device_factory_provider.h"

namespace video_capture {

MockDeviceFactoryProvider::MockDeviceFactoryProvider() {}

MockDeviceFactoryProvider::~MockDeviceFactoryProvider() = default;

void MockDeviceFactoryProvider::ConnectToDeviceFactory(
    video_capture::mojom::DeviceFactoryRequest request) {
  DoConnectToDeviceFactory(request);
}

void MockDeviceFactoryProvider::ConnectToVideoSourceProvider(
    video_capture::mojom::VideoSourceProviderRequest request) {
  DoConnectToVideoSourceProvider(request);
}

#if defined(OS_CHROMEOS)
void MockDeviceFactoryProvider::InjectGpuDependencies(
    video_capture::mojom::AcceleratorFactoryPtr accelerator_factory) {
  DoInjectGpuDependencies(accelerator_factory);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace video_capture
