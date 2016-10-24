// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_

#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/interfaces/video_capture_device_proxy.mojom.h"

namespace video_capture {

// Implementation of mojom::VideoCaptureDeviceProxy backed by a given instance
// of media::VideoCaptureDevice.
class VideoCaptureDeviceProxyImpl : public mojom::VideoCaptureDeviceProxy {
 public:
  VideoCaptureDeviceProxyImpl(std::unique_ptr<media::VideoCaptureDevice> device,
                              const media::VideoCaptureJpegDecoderFactoryCB&
                                  jpeg_decoder_factory_callback);
  ~VideoCaptureDeviceProxyImpl() override;

  // mojom::VideoCaptureDeviceProxy:
  void Start(const media::VideoCaptureFormat& requested_format,
             media::ResolutionChangePolicy resolution_change_policy,
             media::PowerLineFrequency power_line_frequency,
             mojom::VideoFrameReceiverPtr receiver) override;

  void Stop();

  void OnClientConnectionErrorOrClose();

 private:
  const std::unique_ptr<media::VideoCaptureDevice> device_;
  media::VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback_;
  bool device_running_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
