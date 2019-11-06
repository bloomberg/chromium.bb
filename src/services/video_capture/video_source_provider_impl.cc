// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_provider_impl.h"

#include "base/bind.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/video_source_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"

namespace video_capture {

VideoSourceProviderImpl::VideoSourceProviderImpl(
    DeviceFactory* device_factory,
    base::RepeatingClosure on_last_client_disconnected_cb)
    : device_factory_(device_factory),
      on_last_client_disconnected_cb_(
          std::move(on_last_client_disconnected_cb)) {
  // Unretained |this| is safe because |bindings_| is owned by |this|.
  bindings_.set_connection_error_handler(base::BindRepeating(
      &VideoSourceProviderImpl::OnClientDisconnected, base::Unretained(this)));
}

VideoSourceProviderImpl::~VideoSourceProviderImpl() = default;

void VideoSourceProviderImpl::AddClient(
    mojom::VideoSourceProviderRequest request) {
  bindings_.AddBinding(this, std::move(request));
  client_count_++;
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

void VideoSourceProviderImpl::RegisterVirtualDevicesChangedObserver(
    mojom::DevicesChangedObserverPtr observer,
    bool raise_event_if_virtual_devices_already_present) {
  device_factory_->RegisterVirtualDevicesChangedObserver(
      std::move(observer), raise_event_if_virtual_devices_already_present);
}

void VideoSourceProviderImpl::Close(CloseCallback callback) {
  closed_but_not_yet_disconnected_client_count_++;
  // |callback must be run before OnClientDisconnectedOrClosed(), because if the
  // latter leads to the destruction of |this|, the message pipe to the client
  // gets severed, and the callback never makes it through.
  std::move(callback).Run();
  OnClientDisconnectedOrClosed();
}

void VideoSourceProviderImpl::OnClientDisconnected() {
  if (closed_but_not_yet_disconnected_client_count_ > 0) {
    closed_but_not_yet_disconnected_client_count_--;
    return;
  }
  OnClientDisconnectedOrClosed();
}

void VideoSourceProviderImpl::OnClientDisconnectedOrClosed() {
  client_count_--;
  if (client_count_ == 0) {
    // No member access allowed after this call, because it may lead to the
    // destruction of |this|.
    on_last_client_disconnected_cb_.Run();
  }
}

void VideoSourceProviderImpl::OnVideoSourceLastClientDisconnected(
    const std::string& device_id) {
  sources_.erase(device_id);
}

}  // namespace video_capture
