// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using webrtc::BasicDesktopFrame;
using webrtc::DesktopRect;
using webrtc::DesktopSize;

namespace remoting {
namespace protocol {

class WebrtcFrameSchedulerTest : public ::testing::Test {
 public:
  WebrtcFrameSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_.get()),
        now_(base::TimeTicks::Now()) {
    video_encoder_factory_.reset(new WebrtcDummyVideoEncoderFactory());
    scheduler_.reset(new WebrtcFrameSchedulerSimple());
    scheduler_->SetCurrentTimeForTest(now_);
    scheduler_->Start(video_encoder_factory_.get(),
                      base::Bind(&WebrtcFrameSchedulerTest::CaptureCallback,
                                 base::Unretained(this)));
  }
  ~WebrtcFrameSchedulerTest() override {}

  void CaptureCallback() { capture_callback_called_ = true; }

  void VerifyCaptureCallbackCalled() {
    EXPECT_TRUE(capture_callback_called_);
    capture_callback_called_ = false;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  std::unique_ptr<WebrtcDummyVideoEncoderFactory> video_encoder_factory_;
  std::unique_ptr<WebrtcFrameSchedulerSimple> scheduler_;

  bool capture_callback_called_ = false;

  base::TimeTicks now_;
};

TEST_F(WebrtcFrameSchedulerTest, UpdateBitrateWhenPending) {
  auto video_channel_observer =
      video_encoder_factory_->get_video_channel_state_observer_for_tests();

  video_channel_observer->OnKeyFrameRequested();
  video_channel_observer->OnTargetBitrateChanged(100);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunPendingTasks();

  VerifyCaptureCallbackCalled();

  video_channel_observer->OnTargetBitrateChanged(1001);

  // |task_runner_| shouldn't have pending tasks as the scheduler should be
  // waiting for the previous capture request to complete.
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(WebrtcFrameSchedulerTest, EmptyFrameUpdate_ShouldNotBeSentImmediately) {
  auto video_channel_observer =
      video_encoder_factory_->get_video_channel_state_observer_for_tests();
  // Needed to avoid DCHECK in OnFrameCaptured().
  video_channel_observer->OnTargetBitrateChanged(100);

  WebrtcVideoEncoder::FrameParams out_params;
  BasicDesktopFrame frame(DesktopSize(1, 1));
  // Initial capture, full frame.
  frame.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));
  scheduler_->OnFrameCaptured(&frame, &out_params);
  // Empty frame.
  frame.mutable_updated_region()->Clear();
  bool result = scheduler_->OnFrameCaptured(&frame, &out_params);

  // Should not be sent, because of throttling of empty frames.
  EXPECT_FALSE(result);
};

TEST_F(WebrtcFrameSchedulerTest, EmptyFrameUpdate_ShouldBeSentAfter200ms) {
  // Identical to the previous test, except it waits a short amount of time
  // before the empty frame update.
  auto video_channel_observer =
      video_encoder_factory_->get_video_channel_state_observer_for_tests();
  video_channel_observer->OnTargetBitrateChanged(100);

  WebrtcVideoEncoder::FrameParams out_params;
  BasicDesktopFrame frame(DesktopSize(1, 1));
  // Initial capture, full frame.
  frame.mutable_updated_region()->SetRect(DesktopRect::MakeWH(1, 1));
  scheduler_->OnFrameCaptured(&frame, &out_params);
  // Wait more than 200ms.
  scheduler_->SetCurrentTimeForTest(now_ +
                                    base::TimeDelta::FromMilliseconds(300));
  // Empty frame.
  frame.mutable_updated_region()->Clear();
  bool result = scheduler_->OnFrameCaptured(&frame, &out_params);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(300), out_params.duration);

  // Empty frames should be sent at the throttled rate.
  EXPECT_TRUE(result);
};

// TODO(sergeyu): Add more unittests.

}  // namespace protocol
}  // namespace remoting
