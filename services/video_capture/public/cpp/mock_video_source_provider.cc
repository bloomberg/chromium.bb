// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_source_provider.h"

namespace video_capture {

MockVideoSourceProvider::MockVideoSourceProvider() = default;

MockVideoSourceProvider::~MockVideoSourceProvider() = default;

void MockVideoSourceProvider::GetVideoSource(
    const std::string& device_id,
    video_capture::mojom::VideoSourceRequest source_request) {
  DoGetVideoSource(device_id, &source_request);
}

void MockVideoSourceProvider::GetSourceInfos(GetSourceInfosCallback callback) {
  DoGetSourceInfos(callback);
}

void MockVideoSourceProvider::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    video_capture::mojom::ProducerPtr producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    video_capture::mojom::SharedMemoryVirtualDeviceRequest virtual_device) {
  DoAddVirtualDevice(device_info, producer.get(), &virtual_device);
}

void MockVideoSourceProvider::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    video_capture::mojom::TextureVirtualDeviceRequest virtual_device) {
  DoAddTextureVirtualDevice(device_info, &virtual_device);
}

void MockVideoSourceProvider::Close(CloseCallback callback) {
  DoClose(callback);
}

}  // namespace video_capture
