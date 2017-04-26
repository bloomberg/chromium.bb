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

namespace {

// The maximum number of video frame buffers in-flight at any one time.
// If all buffers are still in use by consumers when new frames are produced
// those frames get dropped.
static const int kMaxBufferCount = 3;
}

namespace video_capture {

DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device,
    const media::VideoCaptureJpegDecoderFactoryCB&
        jpeg_decoder_factory_callback)
    : device_(std::move(device)),
      jpeg_decoder_factory_callback_(jpeg_decoder_factory_callback),
      device_running_(false) {}

DeviceMediaToMojoAdapter::~DeviceMediaToMojoAdapter() {
  if (device_running_)
    device_->StopAndDeAllocate();
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
  auto buffer_tracker_factory =
      base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>();
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
      new media::VideoCaptureBufferPoolImpl(std::move(buffer_tracker_factory),
                                            max_buffer_pool_buffer_count()));

  auto device_client = base::MakeUnique<media::VideoCaptureDeviceClient>(
      std::move(media_receiver), buffer_pool, jpeg_decoder_factory_callback_);

  device_->AllocateAndStart(requested_settings, std::move(device_client));
  device_running_ = true;
}

void DeviceMediaToMojoAdapter::OnReceiverReportingUtilization(
    int32_t frame_feedback_id,
    double utilization) {
  device_->OnUtilizationReport(frame_feedback_id, utilization);
}

void DeviceMediaToMojoAdapter::Stop() {
  if (device_running_ == false)
    return;
  device_running_ = false;
  device_->StopAndDeAllocate();
}

void DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose() {
  Stop();
}

// static
int DeviceMediaToMojoAdapter::max_buffer_pool_buffer_count() {
  return kMaxBufferCount;
}

}  // namespace video_capture
