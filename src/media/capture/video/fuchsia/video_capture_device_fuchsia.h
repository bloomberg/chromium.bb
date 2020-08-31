// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_
#define MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_

#include <fuchsia/camera3/cpp/fidl.h>

#include <memory>

#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device.h"
#include "media/fuchsia/common/sysmem_buffer_pool.h"
#include "media/fuchsia/common/sysmem_buffer_reader.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceFuchsia : public VideoCaptureDevice {
 public:
  // Returns pixel format to which video frames in the specified sysmem pixel
  // |format| will be converted. PIXEL_FORMAT_UNKNOWN is returned for
  // unsupported formats.
  static VideoPixelFormat GetConvertedPixelFormat(
      fuchsia::sysmem::PixelFormatType format);

  static bool IsSupportedPixelFormat(fuchsia::sysmem::PixelFormatType format);

  explicit VideoCaptureDeviceFuchsia(
      fidl::InterfaceHandle<fuchsia::camera3::Device> device);
  ~VideoCaptureDeviceFuchsia() final;

  VideoCaptureDeviceFuchsia(const VideoCaptureDeviceFuchsia&) = delete;
  VideoCaptureDeviceFuchsia& operator=(const VideoCaptureDeviceFuchsia&) =
      delete;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) final;
  void StopAndDeAllocate() final;

 private:
  // Disconnects the |stream_| and resets related state.
  void DisconnectStream();

  // Reports the specified |error| to the client.
  void OnError(base::Location location,
               VideoCaptureError error,
               const std::string& reason);

  // Error handlers for the |device_| and |stream_|.
  void OnDeviceError(zx_status_t status);
  void OnStreamError(zx_status_t status);

  // Watches for resolution updates and updates |frame_size_| accordingly.
  void WatchResolution();

  // Callback for WatchResolution().
  void OnWatchResolutionResult(fuchsia::math::Size frame_size);

  // Watches for orientation updates and updates |orientation_| accordingly.
  void WatchOrientation();

  // Callback for WatchOrientation().
  void OnWatchOrientationResult(fuchsia::camera3::Orientation orientation);

  // Watches for sysmem buffer collection updates from the camera.
  void WatchBufferCollection();

  // Initializes buffer collection using the specified token. Initialization is
  // asynchronous. |buffer_reader_| will be set once the initialization is
  // complete. Old buffer collection are dropped synchronously (whether they
  // have finished initialization or not).
  void InitializeBufferCollection(
      fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
          token_handle);

  // Callback for SysmemBufferPool::Creator.
  void OnBufferCollectionCreated(std::unique_ptr<SysmemBufferPool> collection);

  // Callback for SysmemBufferPool::CreateReader().
  void OnBufferReaderCreated(std::unique_ptr<SysmemBufferReader> reader);

  // Calls Stream::GetNextFrame() in a loop to receive incoming frames.
  void ReceiveNextFrame();

  // Processes new frames received by ReceiveNextFrame() and passes it to the
  // |client_|.
  void ProcessNewFrame(fuchsia::camera3::FrameInfo frame_info);

  fuchsia::camera3::DevicePtr device_;
  fuchsia::camera3::StreamPtr stream_;

  std::unique_ptr<Client> client_;

  media::BufferAllocator sysmem_allocator_;
  std::unique_ptr<SysmemBufferPool::Creator> buffer_collection_creator_;
  std::unique_ptr<SysmemBufferPool> buffer_collection_;
  std::unique_ptr<SysmemBufferReader> buffer_reader_;

  base::Optional<gfx::Size> frame_size_;
  fuchsia::camera3::Orientation orientation_ =
      fuchsia::camera3::Orientation::UP;

  base::TimeTicks start_time_;

  bool started_ = false;

  size_t frames_received_ = 0;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FUCHSIA_H_