// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_

#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoSourceProvider
    : public video_capture::mojom::VideoSourceProvider {
 public:
  MockVideoSourceProvider();
  ~MockVideoSourceProvider() override;

  void GetVideoSource(
      const std::string& device_id,
      video_capture::mojom::VideoSourceRequest source_request) override;

  void GetSourceInfos(GetSourceInfosCallback callback) override;

  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      video_capture::mojom::ProducerPtr producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors,
      video_capture::mojom::SharedMemoryVirtualDeviceRequest virtual_device)
      override;

  void AddTextureVirtualDevice(const media::VideoCaptureDeviceInfo& device_info,
                               video_capture::mojom::TextureVirtualDeviceRequest
                                   virtual_device) override;
  void RegisterVirtualDevicesChangedObserver(
      video_capture::mojom::DevicesChangedObserverPtr observer,
      bool raise_event_if_virtual_devices_already_present) override {
    NOTIMPLEMENTED();
  }

  void Close(CloseCallback callback) override;

  MOCK_METHOD1(DoGetSourceInfos, void(GetSourceInfosCallback& callback));
  MOCK_METHOD2(DoGetVideoSource,
               void(const std::string& device_id,
                    video_capture::mojom::VideoSourceRequest* request));
  MOCK_METHOD3(DoAddVirtualDevice,
               void(const media::VideoCaptureDeviceInfo& device_info,
                    video_capture::mojom::ProducerProxy* producer,
                    video_capture::mojom::SharedMemoryVirtualDeviceRequest*
                        virtual_device_request));
  MOCK_METHOD2(
      DoAddTextureVirtualDevice,
      void(const media::VideoCaptureDeviceInfo& device_info,
           video_capture::mojom::TextureVirtualDeviceRequest* virtual_device));
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_
