// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_mock_to_media_adapter.h"

#include "services/video_capture/device_client_media_to_mojo_adapter.h"

namespace video_capture {

DeviceMockToMediaAdapter::DeviceMockToMediaAdapter(
    mojom::MockVideoCaptureDevicePtr device)
    : device_(std::move(device)) {}

DeviceMockToMediaAdapter::~DeviceMockToMediaAdapter() = default;

void DeviceMockToMediaAdapter::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  mojom::MockDeviceClientPtr client_proxy;
  auto client_request = mojo::GetProxy(&client_proxy);

  mojo::MakeStrongBinding(
      base::MakeUnique<DeviceClientMediaToMojoAdapter>(std::move(client)),
      std::move(client_request));

  device_->AllocateAndStart(std::move(client_proxy));
}

void DeviceMockToMediaAdapter::RequestRefreshFrame() {}

void DeviceMockToMediaAdapter::StopAndDeAllocate() {
  device_->StopAndDeAllocate();
}

void DeviceMockToMediaAdapter::GetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {}

void DeviceMockToMediaAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {}

void DeviceMockToMediaAdapter::TakePhoto(TakePhotoCallback callback) {}

}  // namespace video_capture
