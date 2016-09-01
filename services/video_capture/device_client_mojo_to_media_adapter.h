// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MOJO_TO_MEDIA_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MOJO_TO_MEDIA_ADAPTER_H_

#include "media/capture/video/video_capture_device.h"
#include "services/video_capture/public/interfaces/video_capture_device_client.mojom.h"

namespace media {
class MojoSharedBufferVideoFrame;
}

namespace video_capture {

// Adapter that allows a mojom::VideoCaptureDeviceClient to be used in place of
// a media::VideoCaptureDevice::Client.
class DeviceClientMojoToMediaAdapter
    : public media::VideoCaptureDevice::Client {
 public:
  using VideoCaptureFormat = media::VideoCaptureFormat;
  using VideoPixelFormat = media::VideoPixelFormat;
  using VideoPixelStorage = media::VideoPixelStorage;
  using VideoFrame = media::VideoFrame;

  DeviceClientMojoToMediaAdapter(mojom::VideoCaptureDeviceClientPtr client);
  ~DeviceClientMojoToMediaAdapter() override;

  // media::VideoCaptureDevice::Client:
  void OnIncomingCapturedData(const uint8_t* data,
                              int length,
                              const VideoCaptureFormat& frame_format,
                              int clockwise_rotation,
                              base::TimeTicks reference_time,
                              base::TimeDelta timestamp) override;
  std::unique_ptr<Buffer> ReserveOutputBuffer(
      const gfx::Size& dimensions,
      VideoPixelFormat format,
      VideoPixelStorage storage) override;
  void OnIncomingCapturedBuffer(std::unique_ptr<Buffer> buffer,
                                const VideoCaptureFormat& frame_format,
                                base::TimeTicks reference_time,
                                base::TimeDelta timestamp) override;
  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<Buffer> buffer,
      const scoped_refptr<VideoFrame>& frame) override;
  std::unique_ptr<Buffer> ResurrectLastOutputBuffer(
      const gfx::Size& dimensions,
      VideoPixelFormat format,
      VideoPixelStorage storage) override;
  void OnError(const tracked_objects::Location& from_here,
               const std::string& reason) override;
  double GetBufferPoolUtilization() const override;

 private:
  mojom::VideoCaptureDeviceClientPtr client_;
  // TODO(chfremer): Remove when actual implementation is finished.
  // https://crbug.com/584797
  scoped_refptr<media::MojoSharedBufferVideoFrame> dummy_frame_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_CLIENT_MOJO_TO_MEDIA_ADAPTER_H_
