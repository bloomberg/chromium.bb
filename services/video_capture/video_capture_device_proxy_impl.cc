// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/video_capture/device_client_mojo_to_media_adapter.h"
#include "services/video_capture/mojo_media_conversions.h"
#include "services/video_capture/video_capture_device_proxy_impl.h"

namespace video_capture {

VideoCaptureDeviceProxyImpl::VideoCaptureDeviceProxyImpl(
    std::unique_ptr<media::VideoCaptureDevice> device)
    : device_(std::move(device)) {}

VideoCaptureDeviceProxyImpl::~VideoCaptureDeviceProxyImpl() {
  if (device_running_)
    device_->StopAndDeAllocate();
}

void VideoCaptureDeviceProxyImpl::Start(
    const media::VideoCaptureFormat& requested_format,
    mojom::ResolutionChangePolicy resolution_change_policy,
    mojom::PowerLineFrequency power_line_frequency,
    mojom::VideoCaptureDeviceClientPtr client) {
  media::VideoCaptureParams params;
  params.requested_format = requested_format;
  params.resolution_change_policy =
      ConvertFromMojoToMedia(resolution_change_policy);
  params.power_line_frequency = ConvertFromMojoToMedia(power_line_frequency);
  client.set_connection_error_handler(
      base::Bind(&VideoCaptureDeviceProxyImpl::OnClientConnectionErrorOrClose,
                 base::Unretained(this)));
  auto media_client =
      base::MakeUnique<DeviceClientMojoToMediaAdapter>(std::move(client));
  device_->AllocateAndStart(params, std::move(media_client));
  device_running_ = true;
}

void VideoCaptureDeviceProxyImpl::Stop() {
  device_->StopAndDeAllocate();
  device_running_ = false;
}

void VideoCaptureDeviceProxyImpl::OnClientConnectionErrorOrClose() {
  device_->StopAndDeAllocate();
}

}  // namespace video_capture
