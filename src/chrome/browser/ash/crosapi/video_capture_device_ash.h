// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_ASH_H_

#include "base/callback_forward.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device.mojom.h"

namespace crosapi {

class VideoFrameHandlerAsh;

// It is used as a proxy to communicate between Lacros-Chrome and real
// video_capture::Device.
class VideoCaptureDeviceAsh : public crosapi::mojom::VideoCaptureDevice {
 public:
  VideoCaptureDeviceAsh(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDevice> proxy_receiver,
      mojo::PendingRemote<video_capture::mojom::Device> device_remote,
      base::OnceClosure cleanup_callback);
  VideoCaptureDeviceAsh(const VideoCaptureDeviceAsh&) = delete;
  VideoCaptureDeviceAsh& operator=(const VideoCaptureDeviceAsh&) = delete;
  ~VideoCaptureDeviceAsh() override;

 private:
  // crosapi::mojom::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<crosapi::mojom::VideoFrameHandler>
                 proxy_handler) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;

  std::unique_ptr<VideoFrameHandlerAsh> handler_;

  mojo::Receiver<crosapi::mojom::VideoCaptureDevice> receiver_{this};

  mojo::Remote<video_capture::mojom::Device> device_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_VIDEO_CAPTURE_DEVICE_ASH_H_
