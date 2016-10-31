// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "services/video_capture/fake_device_test.h"
#include "services/video_capture/mock_video_frame_receiver.h"
#include "services/video_capture/public/cpp/video_capture_settings.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/video_capture_service_test.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace video_capture {

TEST_F(FakeDeviceTest, FrameCallbacksArrive) {
  VideoCaptureSettings arbitrary_requested_settings;
  arbitrary_requested_settings.format.frame_size.SetSize(640, 480);
  arbitrary_requested_settings.format.frame_rate = 15;
  arbitrary_requested_settings.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  arbitrary_requested_settings.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  base::RunLoop wait_loop;
  const int kNumFramesToWaitFor = 3;
  int num_frames_arrived = 0;
  mojom::VideoFrameReceiverPtr receiver_proxy;
  MockVideoFrameReceiver receiver(mojo::GetProxy(&receiver_proxy));
  EXPECT_CALL(receiver, OnIncomingCapturedVideoFramePtr(_))
      .WillRepeatedly(InvokeWithoutArgs(
          [&wait_loop, &kNumFramesToWaitFor, &num_frames_arrived]() {
            num_frames_arrived += 1;
            if (num_frames_arrived >= kNumFramesToWaitFor) {
              wait_loop.Quit();
            }
          }));

  fake_device_proxy_->Start(arbitrary_requested_settings,
                            std::move(receiver_proxy));
  wait_loop.Run();
}

}  // namespace video_capture
