// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service.h"

#include "base/message_loop/message_loop.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/mock_device_factory.h"

namespace {

// TODO(chfremer): Replace with an actual decoder factory.
// https://crbug.com/584797
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateJpegDecoder() {
  return nullptr;
}

}  // anonymous namespace

namespace video_capture {

VideoCaptureService::VideoCaptureService() : mock_device_factory_(nullptr) {}

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
  mock_factory_bindings_.AddBinding(mock_device_factory_adapter_.get(),
                                    std::move(request));
}

void VideoCaptureService::AddDeviceToMockFactory(
    mojom::MockVideoCaptureDevicePtr device,
    const media::VideoCaptureDeviceDescriptor& descriptor,
    const AddDeviceToMockFactoryCallback& callback) {
  LazyInitializeMockDeviceFactory();
  mock_device_factory_->AddMockDevice(std::move(device), std::move(descriptor));
  callback.Run();
}

void VideoCaptureService::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;

  // Create the platform-specific device factory.
  // Task runner does not seem to actually be used.
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::VideoCaptureDeviceFactory::CreateFactory(
          base::MessageLoop::current()->task_runner());

  device_factory_ = base::MakeUnique<DeviceFactoryMediaToMojoAdapter>(
      std::move(media_device_factory), base::Bind(CreateJpegDecoder));
}

void VideoCaptureService::LazyInitializeFakeDeviceFactory() {
  if (fake_device_factory_)
    return;

  fake_device_factory_ = base::MakeUnique<DeviceFactoryMediaToMojoAdapter>(
      base::MakeUnique<media::FakeVideoCaptureDeviceFactory>(),
      base::Bind(&CreateJpegDecoder));
}

void VideoCaptureService::LazyInitializeMockDeviceFactory() {
  if (mock_device_factory_)
    return;

  auto mock_device_factory = base::MakeUnique<MockDeviceFactory>();
  // We keep a pointer to the MockDeviceFactory as a member so that we can
  // invoke its AddMockDevice(). Ownership of the MockDeviceFactory is moved
  // to the DeviceFactoryMediaToMojoAdapter.
  mock_device_factory_ = mock_device_factory.get();
  mock_device_factory_adapter_ =
      base::MakeUnique<DeviceFactoryMediaToMojoAdapter>(
          std::move(mock_device_factory), base::Bind(&CreateJpegDecoder));
}

}  // namespace video_capture
