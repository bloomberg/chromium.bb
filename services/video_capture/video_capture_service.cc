// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service.h"

#include "services/video_capture/video_capture_device_factory_impl.h"

namespace {
static const char kFakeDeviceDisplayName[] = "Fake Video Capture Device";
static const char kFakeDeviceId[] = "FakeDeviceId";
static const char kFakeModelId[] = "FakeModelId";
}

namespace video_capture {

VideoCaptureService::VideoCaptureService() = default;

VideoCaptureService::~VideoCaptureService() = default;

bool VideoCaptureService::OnConnect(const shell::Identity& remote_identity,
                                    shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::VideoCaptureService>(this);
  return true;
}

void VideoCaptureService::Create(const shell::Identity& remote_identity,
                                 mojom::VideoCaptureServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
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

void VideoCaptureService::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;
  device_factory_ = base::MakeUnique<VideoCaptureDeviceFactoryImpl>();
}

void VideoCaptureService::LazyInitializeFakeDeviceFactory() {
  if (fake_device_factory_)
    return;
  fake_device_factory_ = base::MakeUnique<VideoCaptureDeviceFactoryImpl>();
  auto fake_device_descriptor = mojom::VideoCaptureDeviceDescriptor::New();
  fake_device_descriptor->display_name = kFakeDeviceDisplayName;
  fake_device_descriptor->device_id = kFakeDeviceId;
  fake_device_descriptor->model_id = kFakeModelId;
  fake_device_descriptor->capture_api = mojom::VideoCaptureApi::UNKNOWN;
  fake_device_descriptor->transport_type =
      mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  fake_device_factory_->AddDevice(std::move(fake_device_descriptor),
                                  base::MakeUnique<VideoCaptureDeviceImpl>());
}

}  // namespace video_capture
