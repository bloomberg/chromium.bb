// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/service_impl.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "services/service_manager/public/cpp/service_info.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"

namespace {

// TODO(chfremer): Replace with an actual decoder factory.
// https://crbug.com/584797
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateJpegDecoder() {
  return nullptr;
}

}  // anonymous namespace

namespace video_capture {

ServiceImpl::ServiceImpl() {
  registry_.AddInterface<mojom::Service>(this);
}

ServiceImpl::~ServiceImpl() = default;

void ServiceImpl::OnBindInterface(
    const service_manager::ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(source_info.identity, interface_name,
                          std::move(interface_pipe));
}

void ServiceImpl::Create(const service_manager::Identity& remote_identity,
                         mojom::ServiceRequest request) {
  service_bindings_.AddBinding(this, std::move(request));
}

void ServiceImpl::ConnectToDeviceFactory(mojom::DeviceFactoryRequest request) {
  LazyInitializeDeviceFactory();
  factory_bindings_.AddBinding(device_factory_.get(), std::move(request));
}

void ServiceImpl::ConnectToFakeDeviceFactory(
    mojom::DeviceFactoryRequest request) {
  LazyInitializeFakeDeviceFactory();
  fake_factory_bindings_.AddBinding(fake_device_factory_.get(),
                                    std::move(request));
}

void ServiceImpl::LazyInitializeDeviceFactory() {
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

void ServiceImpl::LazyInitializeFakeDeviceFactory() {
  if (fake_device_factory_)
    return;

  fake_device_factory_ = base::MakeUnique<DeviceFactoryMediaToMojoAdapter>(
      base::MakeUnique<media::FakeVideoCaptureDeviceFactory>(),
      base::Bind(&CreateJpegDecoder));
}

}  // namespace video_capture
