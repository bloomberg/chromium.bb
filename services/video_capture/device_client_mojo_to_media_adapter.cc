// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_client_mojo_to_media_adapter.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace video_capture {

using Buffer = media::VideoCaptureDevice::Client::Buffer;

DeviceClientMojoToMediaAdapter::DeviceClientMojoToMediaAdapter(
    mojom::VideoCaptureDeviceClientPtr client)
    : client_(std::move(client)) {
  const gfx::Size kDummyDimensions(640, 480);
  const base::TimeDelta kDummyTimestamp;
  dummy_frame_ = media::MojoSharedBufferVideoFrame::CreateDefaultI420(
      kDummyDimensions, kDummyTimestamp);
}

DeviceClientMojoToMediaAdapter::~DeviceClientMojoToMediaAdapter() = default;

void DeviceClientMojoToMediaAdapter::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  // TODO(chfremer): convert actual received frame data.
  // Use dummy frame data for now.
  // https://crbug.com/584797
  auto video_frame_ptr =
      media::mojom::VideoFrame::From<scoped_refptr<media::VideoFrame>>(
          dummy_frame_);

  client_->OnFrameAvailable(std::move(video_frame_ptr));
}

std::unique_ptr<Buffer> DeviceClientMojoToMediaAdapter::ReserveOutputBuffer(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    VideoPixelStorage storage) {
  NOTIMPLEMENTED();
  return nullptr;
}

void DeviceClientMojoToMediaAdapter::OnIncomingCapturedBuffer(
    std::unique_ptr<Buffer> buffer,
    const VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  NOTIMPLEMENTED();
}

void DeviceClientMojoToMediaAdapter::OnIncomingCapturedVideoFrame(
    std::unique_ptr<Buffer> buffer,
    const scoped_refptr<VideoFrame>& frame) {
  auto video_frame_ptr = media::mojom::VideoFrame::From(frame);
  client_->OnFrameAvailable(std::move(video_frame_ptr));
}

std::unique_ptr<Buffer>
DeviceClientMojoToMediaAdapter::ResurrectLastOutputBuffer(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    VideoPixelStorage storage) {
  NOTIMPLEMENTED();
  return nullptr;
}

void DeviceClientMojoToMediaAdapter::OnError(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  NOTIMPLEMENTED();
}

double DeviceClientMojoToMediaAdapter::GetBufferPoolUtilization() const {
  NOTIMPLEMENTED();
  return 0.0;
}

}  // namespace video_capture
