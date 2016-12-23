// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/test/mock_device_test.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"

namespace {

std::unique_ptr<media::VideoCaptureJpegDecoder> CreateJpegDecoder() {
  return nullptr;
}

}  // anonymous namespace

namespace video_capture {

MockDevice::MockDevice() = default;

MockDevice::~MockDevice() = default;

void MockDevice::AllocateAndStart(const media::VideoCaptureParams& params,
                                  std::unique_ptr<Client> client) {
  DoAllocateAndStart(params, &client);
}

void MockDevice::GetPhotoCapabilities(GetPhotoCapabilitiesCallback callback) {
  DoGetPhotoCapabilities(&callback);
}

void MockDevice::SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                                 SetPhotoOptionsCallback callback) {
  DoSetPhotoOptions(&settings, &callback);
}

void MockDevice::TakePhoto(TakePhotoCallback callback) {
  DoTakePhoto(&callback);
}

MockDeviceTest::MockDeviceTest() = default;

MockDeviceTest::~MockDeviceTest() = default;

void MockDeviceTest::SetUp() {
  message_loop_ = base::MakeUnique<base::MessageLoop>();
  auto mock_device_factory = base::MakeUnique<MockDeviceFactory>();
  // We keep a pointer to the MockDeviceFactory as a member so that we can
  // invoke its AddMockDevice(). Ownership of the MockDeviceFactory is moved
  // to the DeviceFactoryMediaToMojoAdapter.
  mock_device_factory_ = mock_device_factory.get();
  mock_device_factory_adapter_ =
      base::MakeUnique<DeviceFactoryMediaToMojoAdapter>(
          std::move(mock_device_factory), base::Bind(CreateJpegDecoder));

  mock_factory_binding_ = base::MakeUnique<mojo::Binding<mojom::DeviceFactory>>(
      mock_device_factory_adapter_.get(), mojo::MakeRequest(&factory_));

  media::VideoCaptureDeviceDescriptor mock_descriptor;
  mock_descriptor.device_id = "MockDeviceId";
  mock_device_factory_->AddMockDevice(&mock_device_, mock_descriptor);

  // Obtain the mock device from the factory
  factory_->CreateDevice(
      mock_descriptor.device_id, mojo::MakeRequest(&device_proxy_),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {}));

  requested_settings_.format.frame_size = gfx::Size(800, 600);
  requested_settings_.format.frame_rate = 15;
  requested_settings_.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  requested_settings_.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  mock_receiver_ =
      base::MakeUnique<MockReceiver>(mojo::MakeRequest(&mock_receiver_proxy_));
}

}  // namespace video_capture
