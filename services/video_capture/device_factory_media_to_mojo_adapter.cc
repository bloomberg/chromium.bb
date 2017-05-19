// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_media_to_mojo_adapter.h"

#include <sstream>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"

namespace {

// Translates a set of device infos reported by a VideoCaptureSystem to a set
// of device infos that the video capture service exposes to its client.
// The Video Capture Service instances of VideoCaptureDeviceClient to
// convert the formats provided by the VideoCaptureDevice instances. Here, we
// translate the set of supported formats as reported by the |device_factory_|
// to what will be output by the VideoCaptureDeviceClient we connect to it.
// TODO(chfremer): A cleaner design would be to have this translation
// happen in VideoCaptureDeviceClient, and talk only to VideoCaptureDeviceClient
// instead of VideoCaptureSystem.
static void TranslateDeviceInfos(
    const video_capture::mojom::DeviceFactory::GetDeviceInfosCallback& callback,
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  std::vector<media::VideoCaptureDeviceInfo> translated_device_infos;
  for (const auto& device_info : device_infos) {
    media::VideoCaptureDeviceInfo translated_device_info;
    translated_device_info.descriptor = device_info.descriptor;
    for (const auto& format : device_info.supported_formats) {
      media::VideoCaptureFormat translated_format;
      translated_format.pixel_format =
          (format.pixel_format == media::PIXEL_FORMAT_Y16)
              ? media::PIXEL_FORMAT_Y16
              : media::PIXEL_FORMAT_I420;
      translated_format.frame_size = format.frame_size;
      translated_format.frame_rate = format.frame_rate;
      translated_format.pixel_storage = media::PIXEL_STORAGE_CPU;
      if (base::ContainsValue(translated_device_info.supported_formats,
                              translated_format))
        continue;
      translated_device_info.supported_formats.push_back(translated_format);
    }
    if (translated_device_info.supported_formats.empty())
      continue;
    translated_device_infos.push_back(translated_device_info);
  }
  callback.Run(translated_device_infos);
}

static void DiscardDeviceInfosAndCallContinuation(
    base::Closure continuation,
    const std::vector<media::VideoCaptureDeviceInfo>&) {
  continuation.Run();
}

}  // anonymous namespace

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
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    std::unique_ptr<media::VideoCaptureSystem> capture_system,
    const media::VideoCaptureJpegDecoderFactoryCB&
        jpeg_decoder_factory_callback)
    : service_ref_(std::move(service_ref)),
      capture_system_(std::move(capture_system)),
      jpeg_decoder_factory_callback_(jpeg_decoder_factory_callback),
      has_called_get_device_infos_(false),
      weak_factory_(this) {}

DeviceFactoryMediaToMojoAdapter::~DeviceFactoryMediaToMojoAdapter() = default;

void DeviceFactoryMediaToMojoAdapter::GetDeviceInfos(
    const GetDeviceInfosCallback& callback) {
  capture_system_->GetDeviceInfosAsync(
      base::Bind(&TranslateDeviceInfos, callback));
  has_called_get_device_infos_ = true;
}

void DeviceFactoryMediaToMojoAdapter::CreateDevice(
    const std::string& device_id,
    mojom::DeviceRequest device_request,
    const CreateDeviceCallback& callback) {
  auto active_device_iter = active_devices_by_id_.find(device_id);
  if (active_device_iter != active_devices_by_id_.end()) {
    // The requested device is already in use.
    // Revoke the access and close the device, then bind to the new request.
    ActiveDeviceEntry& device_entry = active_device_iter->second;
    device_entry.binding->Unbind();
    device_entry.device->Stop();
    device_entry.binding->Bind(std::move(device_request));
    device_entry.binding->set_connection_error_handler(base::Bind(
        &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
        base::Unretained(this), device_id));
    callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  const auto create_and_add_new_device_cb =
      base::Bind(&DeviceFactoryMediaToMojoAdapter::CreateAndAddNewDevice,
                 weak_factory_.GetWeakPtr(), device_id,
                 base::Passed(&device_request), callback);

  if (has_called_get_device_infos_) {
    create_and_add_new_device_cb.Run();
    return;
  }

  capture_system_->GetDeviceInfosAsync(base::Bind(
      &DiscardDeviceInfosAndCallContinuation, create_and_add_new_device_cb));
}

void DeviceFactoryMediaToMojoAdapter::CreateAndAddNewDevice(
    const std::string& device_id,
    mojom::DeviceRequest device_request,
    const CreateDeviceCallback& callback) {
  std::unique_ptr<media::VideoCaptureDevice> media_device =
      capture_system_->CreateDevice(device_id);
  if (media_device == nullptr) {
    callback.Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
    return;
  }

  // Add entry to active_devices to keep track of it
  ActiveDeviceEntry device_entry;
  device_entry.device = base::MakeUnique<DeviceMediaToMojoAdapter>(
      service_ref_->Clone(), std::move(media_device),
      jpeg_decoder_factory_callback_);
  device_entry.binding = base::MakeUnique<mojo::Binding<mojom::Device>>(
      device_entry.device.get(), std::move(device_request));
  device_entry.binding->set_connection_error_handler(base::Bind(
      &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
      base::Unretained(this), device_id));
  active_devices_by_id_[device_id] = std::move(device_entry);

  callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
}

void DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  active_devices_by_id_[device_id].device->Stop();
  active_devices_by_id_.erase(device_id);
}

}  // namespace video_capture
