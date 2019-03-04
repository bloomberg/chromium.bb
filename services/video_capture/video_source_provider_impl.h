// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_

#include <map>

#include "services/video_capture/device_factory.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace video_capture {

class VideoSourceImpl;

class VideoSourceProviderImpl : public mojom::VideoSourceProvider {
 public:
  explicit VideoSourceProviderImpl(DeviceFactory* device_factory);
  ~VideoSourceProviderImpl() override;

  void SetServiceRef(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);

  // mojom::VideoSourceProvider implementation.
  void GetSourceInfos(GetSourceInfosCallback callback) override;
  void GetVideoSource(const std::string& device_id,
                      mojom::VideoSourceRequest source_request) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      mojom::SharedMemoryVirtualDeviceRequest virtual_device) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::TextureVirtualDeviceRequest virtual_device) override;

 private:
  void OnVideoSourceLastClientDisconnected(const std::string& device_id);

  DeviceFactory* const device_factory_;
  std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  std::map<std::string, std::unique_ptr<VideoSourceImpl>> sources_;
  DISALLOW_COPY_AND_ASSIGN(VideoSourceProviderImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
