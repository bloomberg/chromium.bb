// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service.h"

#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "services/video_capture/video_capture_device_factory_impl.h"

namespace {
static const char kFakeDeviceDisplayName[] = "Fake Video Capture Device";
static const char kFakeDeviceId[] = "FakeDeviceId";
static const char kFakeModelId[] = "FakeModelId";
static const float kFakeCaptureDefaultFrameRate = 20.0f;

// TODO(chfremer): Replace with an actual decoder factory.
// https://crbug.com/584797
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateJpegDecoder() {
  return nullptr;
}

}  // anonymous namespace

namespace video_capture {

VideoCaptureService::VideoCaptureService() = default;

VideoCaptureService::~VideoCaptureService() = default;

bool VideoCaptureService::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::VideoCaptureService>(this);
  return true;
}

void VideoCaptureService::Create(
    const service_manager::Identity& remote_identity,
    mojom::VideoCaptureServiceRequest request) {
  service_bindings_.AddBinding(this, std::move(request));
}

void VideoCaptureService::ConnectToDeviceFactory(
    mojom::VideoCaptureDeviceFactoryRequest request) {
  LazyInitializeDeviceFactory();
  factory_bindings_.AddBinding(device_factory_.get(), std::move(request));
}

void VideoCaptureService::ConnectToFakeDeviceFactory(
    mojom::VideoCaptureDeviceFactoryRequest request) {
  LazyInitializeFakeDeviceFactory();
  fake_factory_bindings_.AddBinding(fake_device_factory_.get(),
                                    std::move(request));
}

void VideoCaptureService::ConnectToMockDeviceFactory(
    mojom::VideoCaptureDeviceFactoryRequest request) {
  LazyInitializeMockDeviceFactory();
  mock_factory_bindings_.AddBinding(mock_device_factory_.get(),
                                    std::move(request));
}

void VideoCaptureService::AddDeviceToMockFactory(
    mojom::MockVideoCaptureDevicePtr device,
    mojom::VideoCaptureDeviceDescriptorPtr descriptor,
    const AddDeviceToMockFactoryCallback& callback) {
  LazyInitializeMockDeviceFactory();
  mock_device_factory_->AddMockDevice(std::move(device), std::move(descriptor));
  callback.Run();
}

void VideoCaptureService::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;
  device_factory_ = base::MakeUnique<VideoCaptureDeviceFactoryImpl>(
      base::Bind(CreateJpegDecoder));
}

void VideoCaptureService::LazyInitializeFakeDeviceFactory() {
  if (fake_device_factory_)
    return;
  fake_device_factory_ = base::MakeUnique<VideoCaptureDeviceFactoryImpl>(
      base::Bind(CreateJpegDecoder));
  auto fake_device_descriptor = mojom::VideoCaptureDeviceDescriptor::New();
  fake_device_descriptor->display_name = kFakeDeviceDisplayName;
  fake_device_descriptor->device_id = kFakeDeviceId;
  fake_device_descriptor->model_id = kFakeModelId;
  fake_device_descriptor->capture_api = mojom::VideoCaptureApi::UNKNOWN;
  fake_device_descriptor->transport_type =
      mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  fake_device_factory_->AddMediaDevice(
      base::MakeUnique<media::FakeVideoCaptureDevice>(
          media::FakeVideoCaptureDevice::BufferOwnership::OWN_BUFFERS,
          kFakeCaptureDefaultFrameRate),
      std::move(fake_device_descriptor));
}

void VideoCaptureService::LazyInitializeMockDeviceFactory() {
  if (mock_device_factory_)
    return;
  mock_device_factory_ = base::MakeUnique<VideoCaptureDeviceFactoryImpl>(
      base::Bind(CreateJpegDecoder));
}

}  // namespace video_capture
