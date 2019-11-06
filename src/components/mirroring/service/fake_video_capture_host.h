// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_

#include <string>

#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class FakeVideoCaptureHost final : public media::mojom::VideoCaptureHost {
 public:
  explicit FakeVideoCaptureHost(media::mojom::VideoCaptureHostRequest request);
  ~FakeVideoCaptureHost() override;

  // mojom::VideoCaptureHost implementations
  MOCK_METHOD1(RequestRefreshFrame, void(const base::UnguessableToken&));
  MOCK_METHOD3(ReleaseBuffer,
               void(const base::UnguessableToken&, int32_t, double));
  MOCK_METHOD1(Pause, void(const base::UnguessableToken&));
  MOCK_METHOD3(Resume,
               void(const base::UnguessableToken&,
                    const base::UnguessableToken&,
                    const media::VideoCaptureParams&));
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD2(OnLog, void(const base::UnguessableToken&, const std::string&));
  MOCK_METHOD2(OnFrameDropped,
               void(const base::UnguessableToken&,
                    media::VideoCaptureFrameDropReason));

  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             media::mojom::VideoCaptureObserverPtr observer) override;
  void Stop(const base::UnguessableToken& device_id) override;

  void GetDeviceSupportedFormats(
      const base::UnguessableToken& device_id,
      const base::UnguessableToken& session_id,
      GetDeviceSupportedFormatsCallback callback) override {}
  void GetDeviceFormatsInUse(const base::UnguessableToken& device_id,
                             const base::UnguessableToken& session_id,
                             GetDeviceFormatsInUseCallback callback) override {}

  // Create one video frame and send it to |observer_|.
  void SendOneFrame(const gfx::Size& size, base::TimeTicks capture_time);

 private:
  mojo::Binding<media::mojom::VideoCaptureHost> binding_;
  media::mojom::VideoCaptureObserverPtr observer_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoCaptureHost);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
