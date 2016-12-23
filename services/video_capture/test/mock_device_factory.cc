// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "services/video_capture/test/mock_device_factory.h"

namespace {

// Report a single hard-coded supported format to clients.
media::VideoCaptureFormat kSupportedFormat(gfx::Size(),
                                           25.0f,
                                           media::PIXEL_FORMAT_I420,
                                           media::PIXEL_STORAGE_CPU);

class RawPointerVideoCaptureDevice : public media::VideoCaptureDevice {
 public:
  explicit RawPointerVideoCaptureDevice(media::VideoCaptureDevice* device)
      : device_(device) {}

  // media::VideoCaptureDevice:
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    device_->AllocateAndStart(params, std::move(client));
  }
  void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }
  void StopAndDeAllocate() override { device_->StopAndDeAllocate(); }
  void GetPhotoCapabilities(GetPhotoCapabilitiesCallback callback) override {
    device_->GetPhotoCapabilities(std::move(callback));
  }
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
  }
  void TakePhoto(TakePhotoCallback callback) override {
    device_->TakePhoto(std::move(callback));
  }

 private:
  media::VideoCaptureDevice* device_;
};

}  // anonymous namespace

namespace video_capture {

MockDeviceFactory::MockDeviceFactory() = default;

MockDeviceFactory::~MockDeviceFactory() = default;

void MockDeviceFactory::AddMockDevice(
    media::VideoCaptureDevice* device,
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  devices_[descriptor] = std::move(device);
}

std::unique_ptr<media::VideoCaptureDevice> MockDeviceFactory::CreateDevice(
    const media::VideoCaptureDeviceDescriptor& device_descriptor) {
  if (devices_.find(device_descriptor) == devices_.end())
    return nullptr;
  return base::MakeUnique<RawPointerVideoCaptureDevice>(
      devices_[device_descriptor]);
}

void MockDeviceFactory::GetDeviceDescriptors(
    media::VideoCaptureDeviceDescriptors* device_descriptors) {
  for (const auto& entry : devices_)
    device_descriptors->push_back(entry.first);
}

void MockDeviceFactory::GetSupportedFormats(
    const media::VideoCaptureDeviceDescriptor& device_descriptor,
    media::VideoCaptureFormats* supported_formats) {
  supported_formats->push_back(kSupportedFormat);
}

}  // namespace video_capture
