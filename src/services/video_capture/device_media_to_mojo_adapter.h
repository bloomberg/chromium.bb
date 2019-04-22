// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_

#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video/video_capture_jpeg_decoder.h"
#include "media/capture/video_capture_types.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/video_capture/public/mojom/device.mojom.h"

namespace video_capture {

class ReceiverMojoToMediaAdapter;

// Implementation of mojom::Device backed by a given instance of
// media::VideoCaptureDevice.
class DeviceMediaToMojoAdapter : public mojom::Device {
 public:
  DeviceMediaToMojoAdapter(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      std::unique_ptr<media::VideoCaptureDevice> device,
      media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner);
  ~DeviceMediaToMojoAdapter() override;

  // mojom::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojom::ReceiverPtr receiver) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

  void Stop();
  void OnClientConnectionErrorOrClose();

  // Returns the fixed maximum number of buffers passed to the constructor
  // of VideoCaptureBufferPoolImpl.
  static int max_buffer_pool_buffer_count();

 private:
  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  const std::unique_ptr<media::VideoCaptureDevice> device_;
  const media::MojoMjpegDecodeAcceleratorFactoryCB
      jpeg_decoder_factory_callback_;
  scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner_;
  std::unique_ptr<ReceiverMojoToMediaAdapter> receiver_;
  bool device_started_;
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DeviceMediaToMojoAdapter> weak_factory_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
