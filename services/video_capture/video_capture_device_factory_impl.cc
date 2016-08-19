// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "services/video_capture/video_capture_device_factory_impl.h"

namespace video_capture {

VideoCaptureDeviceFactoryImpl::DeviceEntry::DeviceEntry(
    mojom::VideoCaptureDeviceDescriptorPtr descriptor,
    std::unique_ptr<VideoCaptureDeviceImpl> bindable_target)
    : descriptor_(std::move(descriptor)), device_(std::move(bindable_target)) {}

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

VideoCaptureDeviceFactoryImpl::VideoCaptureDeviceFactoryImpl() = default;

VideoCaptureDeviceFactoryImpl::~VideoCaptureDeviceFactoryImpl() = default;

void VideoCaptureDeviceFactoryImpl::AddDevice(
    mojom::VideoCaptureDeviceDescriptorPtr descriptor,
    std::unique_ptr<VideoCaptureDeviceImpl> device) {
  devices_.emplace_back(std::move(descriptor), std::move(device));
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

void VideoCaptureDeviceFactoryImpl::CreateDevice(
    mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
    mojom::VideoCaptureDeviceRequest request,
    const CreateDeviceCallback& callback) {
  callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
}

}  // namespace video_capture
