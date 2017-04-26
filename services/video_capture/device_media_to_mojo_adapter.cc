// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "services/video_capture/receiver_mojo_to_media_adapter.h"

namespace video_capture {

DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device,
    const media::VideoCaptureJpegDecoderFactoryCB&
        jpeg_decoder_factory_callback)
    : device_(std::move(device)),
      jpeg_decoder_factory_callback_(jpeg_decoder_factory_callback),
      device_running_(false) {}

DeviceMediaToMojoAdapter::~DeviceMediaToMojoAdapter() {
  Stop();
}

void DeviceMediaToMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojom::ReceiverPtr receiver) {
  receiver.set_connection_error_handler(
      base::Bind(&DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose,
                 base::Unretained(this)));

  auto media_receiver =
      base::MakeUnique<ReceiverMojoToMediaAdapter>(std::move(receiver));

  // Create a dedicated buffer pool for the device usage session.
  const int kMaxBufferCount = 2;
  auto buffer_tracker_factory =
      base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>();
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
      new media::VideoCaptureBufferPoolImpl(std::move(buffer_tracker_factory),
                                            kMaxBufferCount));

  auto device_client = base::MakeUnique<media::VideoCaptureDeviceClient>(
      std::move(media_receiver), buffer_pool, jpeg_decoder_factory_callback_);

  device_->AllocateAndStart(requested_settings, std::move(device_client));
  device_running_ = true;
}

void DeviceMediaToMojoAdapter::Stop() {
  if (device_running_ == false)
    return;
  device_->StopAndDeAllocate();
  device_running_ = false;
}

void DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose() {
  Stop();
}

}  // namespace video_capture
