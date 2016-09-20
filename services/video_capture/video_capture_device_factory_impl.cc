// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_device_factory_impl.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "services/video_capture/device_mock_to_media_adapter.h"

namespace video_capture {

VideoCaptureDeviceFactoryImpl::DeviceEntry::DeviceEntry(
    mojom::VideoCaptureDeviceDescriptorPtr descriptor,
    std::unique_ptr<VideoCaptureDeviceProxyImpl> bindable_target)
    : descriptor_(std::move(descriptor)),
      binding_(base::MakeUnique<mojo::Binding<mojom::VideoCaptureDeviceProxy>>(
          bindable_target.get())) {
  device_proxy_ = std::move(bindable_target);
}

VideoCaptureDeviceFactoryImpl::DeviceEntry::~DeviceEntry() = default;

VideoCaptureDeviceFactoryImpl::DeviceEntry::DeviceEntry(
    VideoCaptureDeviceFactoryImpl::DeviceEntry&& other) = default;

VideoCaptureDeviceFactoryImpl::DeviceEntry&
VideoCaptureDeviceFactoryImpl::DeviceEntry::operator=(
    VideoCaptureDeviceFactoryImpl::DeviceEntry&& other) = default;

mojom::VideoCaptureDeviceDescriptorPtr
VideoCaptureDeviceFactoryImpl::DeviceEntry::MakeDescriptorCopy() const {
  return descriptor_.Clone();
}

bool VideoCaptureDeviceFactoryImpl::DeviceEntry::DescriptorEquals(
    const mojom::VideoCaptureDeviceDescriptorPtr& other) const {
  return descriptor_.Equals(other);
}

bool VideoCaptureDeviceFactoryImpl::DeviceEntry::is_bound() const {
  return binding_->is_bound();
}

void VideoCaptureDeviceFactoryImpl::DeviceEntry::Bind(
    mojom::VideoCaptureDeviceProxyRequest request) {
  binding_->Bind(std::move(request));
  binding_->set_connection_error_handler(base::Bind(
      &VideoCaptureDeviceFactoryImpl::DeviceEntry::OnConnectionErrorOrClose,
      base::Unretained(this)));
}

void VideoCaptureDeviceFactoryImpl::DeviceEntry::Unbind() {
  binding_->Unbind();
  device_proxy_->Stop();
}

void VideoCaptureDeviceFactoryImpl::DeviceEntry::OnConnectionErrorOrClose() {
  Unbind();
}

VideoCaptureDeviceFactoryImpl::VideoCaptureDeviceFactoryImpl() = default;

VideoCaptureDeviceFactoryImpl::~VideoCaptureDeviceFactoryImpl() = default;

void VideoCaptureDeviceFactoryImpl::AddMojoDevice(
    std::unique_ptr<VideoCaptureDeviceProxyImpl> device,
    mojom::VideoCaptureDeviceDescriptorPtr descriptor) {
  devices_.emplace_back(std::move(descriptor), std::move(device));
}

void VideoCaptureDeviceFactoryImpl::AddMediaDevice(
    std::unique_ptr<media::VideoCaptureDevice> device,
    mojom::VideoCaptureDeviceDescriptorPtr descriptor) {
  AddMojoDevice(
      base::MakeUnique<VideoCaptureDeviceProxyImpl>(std::move(device)),
      std::move(descriptor));
}

void VideoCaptureDeviceFactoryImpl::AddMockDevice(
    mojom::MockVideoCaptureDevicePtr device,
    mojom::VideoCaptureDeviceDescriptorPtr descriptor) {
  AddMediaDevice(base::MakeUnique<DeviceMockToMediaAdapter>(std::move(device)),
                 std::move(descriptor));
}

void VideoCaptureDeviceFactoryImpl::EnumerateDeviceDescriptors(
    const EnumerateDeviceDescriptorsCallback& callback) {
  std::vector<mojom::VideoCaptureDeviceDescriptorPtr> descriptors;
  for (const auto& entry : devices_)
    descriptors.push_back(entry.MakeDescriptorCopy());
  callback.Run(std::move(descriptors));
}

void VideoCaptureDeviceFactoryImpl::GetSupportedFormats(
    mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
    const GetSupportedFormatsCallback& callback) {
  NOTIMPLEMENTED();
}

void VideoCaptureDeviceFactoryImpl::CreateDeviceProxy(
    mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
    mojom::VideoCaptureDeviceProxyRequest proxy_request,
    const CreateDeviceProxyCallback& callback) {
  for (auto& entry : devices_) {
    if (entry.DescriptorEquals(device_descriptor)) {
      if (entry.is_bound())
        entry.Unbind();
      entry.Bind(std::move(proxy_request));
      callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
      return;
    }
  }
  callback.Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
}

}  // namespace video_capture
