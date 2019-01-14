// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_provider_impl.h"

#include "services/video_capture/video_source_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"

namespace video_capture {

VideoSourceProviderImpl::VideoSourceProviderImpl(DeviceFactory* device_factory)
    : device_factory_(device_factory) {}

VideoSourceProviderImpl::~VideoSourceProviderImpl() = default;

void VideoSourceProviderImpl::SetServiceRef(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  if (service_ref)
    device_factory_->SetServiceRef(service_ref->Clone());
  else
    device_factory_->SetServiceRef(nullptr);
  service_ref_ = std::move(service_ref);
}

void VideoSourceProviderImpl::GetSourceInfos(GetSourceInfosCallback callback) {
  device_factory_->GetDeviceInfos(std::move(callback));
}

void VideoSourceProviderImpl::GetVideoSource(
    const std::string& device_id,
    mojom::VideoSourceRequest source_request) {
  auto source_iter = sources_.find(device_id);
  if (source_iter == sources_.end()) {
    auto video_source = std::make_unique<VideoSourceImpl>(
        device_factory_, device_id,
        base::BindRepeating(
            &VideoSourceProviderImpl::OnVideoSourceLastClientDisconnected,
            base::Unretained(this), device_id));
    source_iter =
        sources_.insert(std::make_pair(device_id, std::move(video_source)))
            .first;
  }
  source_iter->second->AddToBindingSet(std::move(source_request));
}

void VideoSourceProviderImpl::AddSharedMemoryVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojom::ProducerPtr producer,
    bool send_buffer_handles_to_producer_as_raw_file_descriptors,
    mojom::SharedMemoryVirtualDeviceRequest virtual_device) {
  device_factory_->AddSharedMemoryVirtualDevice(
      device_info, std::move(producer),
      send_buffer_handles_to_producer_as_raw_file_descriptors,
      std::move(virtual_device));
}

void VideoSourceProviderImpl::AddTextureVirtualDevice(
    const media::VideoCaptureDeviceInfo& device_info,
    mojom::TextureVirtualDeviceRequest virtual_device) {
  device_factory_->AddTextureVirtualDevice(device_info,
                                           std::move(virtual_device));
}

void VideoSourceProviderImpl::OnVideoSourceLastClientDisconnected(
    const std::string& device_id) {
  sources_.erase(device_id);
}

}  // namespace video_capture
