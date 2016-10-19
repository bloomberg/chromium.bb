// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class WebrtcFrameSchedulerTest : public ::testing::Test {
 public:
  WebrtcFrameSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_.get()) {
    video_encoder_factory_.reset(new WebrtcDummyVideoEncoderFactory());
    scheduler_.reset(new WebrtcFrameSchedulerSimple());
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
  std::unique_ptr<WebrtcFrameScheduler> scheduler_;

  bool capture_callback_called_ = false;
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

// TODO(sergeyu): Add more unittests.

}  // namespace protocol
}  // namespace remoting
