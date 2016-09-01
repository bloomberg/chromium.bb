// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_

#include "media/capture/video/video_capture_device.h"
#include "services/video_capture/public/interfaces/video_capture_device_proxy.mojom.h"

namespace video_capture {

// Implementation of mojom::VideoCaptureDeviceProxy backed by a given instance
// of media::VideoCaptureDevice.
class VideoCaptureDeviceProxyImpl : public mojom::VideoCaptureDeviceProxy {
 public:
  VideoCaptureDeviceProxyImpl(
      std::unique_ptr<media::VideoCaptureDevice> device);
  ~VideoCaptureDeviceProxyImpl() override;

  // mojom::VideoCaptureDeviceProxy:
  void Start(mojom::VideoCaptureFormatPtr requested_format,
             mojom::ResolutionChangePolicy resolution_change_policy,
             mojom::PowerLineFrequency power_line_frequency,
             mojom::VideoCaptureDeviceClientPtr client) override;

  // TODO(chfremer): Consider using Mojo type mapping instead of conversion
  // methods.
  // https://crbug.com/642387
  static media::VideoCaptureFormat ConvertFromMojoToMedia(
      mojom::VideoCaptureFormatPtr format);
  static media::VideoPixelFormat ConvertFromMojoToMedia(
      media::mojom::VideoFormat format);
  static media::VideoPixelStorage ConvertFromMojoToMedia(
      mojom::VideoPixelStorage storage);
  static media::ResolutionChangePolicy ConvertFromMojoToMedia(
      mojom::ResolutionChangePolicy policy);
  static media::PowerLineFrequency ConvertFromMojoToMedia(
      mojom::PowerLineFrequency frequency);

 private:
  std::unique_ptr<media::VideoCaptureDevice> device_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
