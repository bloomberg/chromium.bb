// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/lacros/device_proxy_lacros.h"

#include <memory>
#include <utility>

#include "media/capture/mojom/image_capture.mojom.h"
#include "services/video_capture/lacros/video_frame_handler_proxy_lacros.h"

namespace video_capture {

DeviceProxyLacros::DeviceProxyLacros(
    mojo::PendingReceiver<mojom::Device> device_receiver,
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDevice> proxy_remote,
    base::OnceClosure cleanup_callback)
    : device_(std::move(proxy_remote)) {
  receiver_.Bind(std::move(device_receiver));
  receiver_.set_disconnect_handler(std::move(cleanup_callback));
}

DeviceProxyLacros::~DeviceProxyLacros() = default;

void DeviceProxyLacros::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler> handler) {
  mojo::PendingRemote<crosapi::mojom::VideoFrameHandler> proxy_handler_remote;
  handler_ = std::make_unique<VideoFrameHandlerProxyLacros>(
      proxy_handler_remote.InitWithNewPipeAndPassReceiver(),
      std::move(handler));
  device_->Start(std::move(requested_settings),
                 std::move(proxy_handler_remote));
}

void DeviceProxyLacros::MaybeSuspend() {
  device_->MaybeSuspend();
}

void DeviceProxyLacros::Resume() {
  device_->Resume();
}

void DeviceProxyLacros::GetPhotoState(GetPhotoStateCallback callback) {
  device_->GetPhotoState(std::move(callback));
}

void DeviceProxyLacros::SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                                        SetPhotoOptionsCallback callback) {
  device_->SetPhotoOptions(std::move(settings), std::move(callback));
}

void DeviceProxyLacros::TakePhoto(TakePhotoCallback callback) {
  device_->TakePhoto(std::move(callback));
}

void DeviceProxyLacros::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  device_->ProcessFeedback(std::move(feedback));
}

}  // namespace video_capture
