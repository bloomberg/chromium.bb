// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_MOJO_ADAPTER_H_

#include "base/sequence_checker.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video_capture_types.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/interfaces/device.mojom.h"
#include "services/video_capture/public/interfaces/producer.mojom.h"
#include "services/video_capture/public/interfaces/virtual_device.mojom.h"

namespace video_capture {

// Implementation of mojom::Device backed by mojom::VirtualDevice.
class VirtualDeviceMojoAdapter : public mojom::VirtualDevice,
                                 public mojom::Device {
 public:
  VirtualDeviceMojoAdapter(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      const media::VideoCaptureDeviceInfo& device_info,
      mojom::ProducerPtr producer);
  ~VirtualDeviceMojoAdapter() override;

  // mojom::VirtualDevice implementation.
  void RequestFrameBuffer(const gfx::Size& dimension,
                          media::VideoPixelFormat pixel_format,
                          media::VideoPixelStorage pixel_storage,
                          RequestFrameBufferCallback callback) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      ::media::mojom::VideoFrameInfoPtr frame_info) override;

  // mojom::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojom::ReceiverPtr receiver) override;
  void OnReceiverReportingUtilization(int32_t frame_feedback_id,
                                      double utilization) override;
  void RequestRefreshFrame() override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

  void Stop();

  const media::VideoCaptureDeviceInfo& device_info() const {
    return device_info_;
  }

  // Returns the fixed maximum number of buffers passed to the constructor
  // of VideoCaptureBufferPoolImpl.
  static int max_buffer_pool_buffer_count();

 private:
  void OnReceiverConnectionErrorOrClose();

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  const media::VideoCaptureDeviceInfo device_info_;
  mojom::ReceiverPtr receiver_;
  mojom::ProducerPtr producer_;
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
  std::vector<int> known_buffer_ids_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VirtualDeviceMojoAdapter);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
