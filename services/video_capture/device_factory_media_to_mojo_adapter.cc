// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_media_to_mojo_adapter.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/device_mock_to_media_adapter.h"
#include "services/video_capture/video_capture_device_proxy_impl.h"

namespace video_capture {

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::~ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&
DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::operator=(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

DeviceFactoryMediaToMojoAdapter::DeviceFactoryMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory,
    const media::VideoCaptureJpegDecoderFactoryCB&
        jpeg_decoder_factory_callback)
    : device_factory_(std::move(device_factory)),
      jpeg_decoder_factory_callback_(jpeg_decoder_factory_callback) {}

DeviceFactoryMediaToMojoAdapter::~DeviceFactoryMediaToMojoAdapter() = default;

void DeviceFactoryMediaToMojoAdapter::EnumerateDeviceDescriptors(
    const EnumerateDeviceDescriptorsCallback& callback) {
  media::VideoCaptureDeviceDescriptors descriptors;
  device_factory_->GetDeviceDescriptors(&descriptors);
  callback.Run(descriptors);
}

void DeviceFactoryMediaToMojoAdapter::GetSupportedFormats(
    const media::VideoCaptureDeviceDescriptor& device_descriptor,
    const GetSupportedFormatsCallback& callback) {
  std::vector<VideoCaptureFormat> result;
  std::vector<media::VideoCaptureFormat> media_formats;
  device_factory_->GetSupportedFormats(device_descriptor, &media_formats);
  for (const auto& media_format : media_formats) {
    // The Video Capture Service requires devices to deliver frames either in
    // I420 or MJPEG formats.
    // TODO(chfremer): Add support for Y16 format. See crbug.com/624436.
    if (media_format.pixel_format != media::PIXEL_FORMAT_I420 &&
        media_format.pixel_format != media::PIXEL_FORMAT_MJPEG) {
      continue;
    }
    VideoCaptureFormat format;
    format.frame_size = media_format.frame_size;
    format.frame_rate = media_format.frame_rate;
    if (base::ContainsValue(result, format))
      continue;  // Result already contains this format
    result.push_back(format);
  }
  callback.Run(std::move(result));
}

void DeviceFactoryMediaToMojoAdapter::CreateDeviceProxy(
    const media::VideoCaptureDeviceDescriptor& device_descriptor,
    mojom::VideoCaptureDeviceProxyRequest proxy_request,
    const CreateDeviceProxyCallback& callback) {
  if (active_devices_.find(device_descriptor) != active_devices_.end()) {
    // The requested device is already in use.
    // Revoke the access and close the device, then bind to the new request.
    ActiveDeviceEntry& device_entry = active_devices_[device_descriptor];
    device_entry.binding->Unbind();
    device_entry.device_proxy->Stop();
    device_entry.binding->Bind(std::move(proxy_request));
    device_entry.binding->set_connection_error_handler(base::Bind(
        &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
        base::Unretained(this), device_descriptor));
    callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  std::unique_ptr<media::VideoCaptureDevice> media_device =
      device_factory_->CreateDevice(device_descriptor);
  if (media_device == nullptr) {
    callback.Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
    return;
  }

  // Add entry to |active_devices| to keep track of it
  ActiveDeviceEntry device_entry;
  device_entry.device_proxy = base::MakeUnique<VideoCaptureDeviceProxyImpl>(
      std::move(media_device), jpeg_decoder_factory_callback_);
  device_entry.binding =
      base::MakeUnique<mojo::Binding<mojom::VideoCaptureDeviceProxy>>(
          device_entry.device_proxy.get(), std::move(proxy_request));
  device_entry.binding->set_connection_error_handler(base::Bind(
      &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
      base::Unretained(this), device_descriptor));
  active_devices_[device_descriptor] = std::move(device_entry);

  callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
}

void DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  active_devices_[descriptor].device_proxy->Stop();
  active_devices_.erase(descriptor);
}

}  // namespace video_capture
