// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_scheduler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/host/fake_screen_capturer.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

using ::remoting::protocol::MockClientStub;
using ::remoting::protocol::MockVideoStub;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace remoting {

namespace {

ACTION(FinishEncode) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  return packet.release();
}

ACTION(FinishSend) {
  arg1.Run();
}

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder() {}
  virtual ~MockVideoEncoder() {}

  scoped_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) {
    return scoped_ptr<VideoPacket>(EncodePtr(frame));
  }
  MOCK_METHOD1(EncodePtr, VideoPacket*(const webrtc::DesktopFrame& frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoEncoder);
};

class ThreadCheckVideoEncoder : public VideoEncoderVerbatim {
 public:
  ThreadCheckVideoEncoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
  }
  virtual ~ThreadCheckVideoEncoder() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckVideoEncoder);
};

class ThreadCheckScreenCapturer : public FakeScreenCapturer {
 public:
  ThreadCheckScreenCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
  }
  virtual ~ThreadCheckScreenCapturer() {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckScreenCapturer);
};

class VideoSchedulerTest : public testing::Test {
 public:
  VideoSchedulerTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void StartVideoScheduler(
      scoped_ptr<webrtc::ScreenCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder);
  void StopVideoScheduler();

  // webrtc::ScreenCapturer mocks.
  void OnCapturerStart(webrtc::ScreenCapturer::Callback* callback);
  void OnCaptureFrame(const webrtc::DesktopRegion& region);

 protected:
  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> capture_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> encode_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  scoped_refptr<VideoScheduler> scheduler_;

  MockClientStub client_stub_;
  MockVideoStub video_stub_;

  scoped_ptr<webrtc::DesktopFrame> frame_;

  // Points to the callback passed to webrtc::ScreenCapturer::Start().
  webrtc::ScreenCapturer::Callback* capturer_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoSchedulerTest);
};

VideoSchedulerTest::VideoSchedulerTest()
    : capturer_callback_(NULL) {
}

void VideoSchedulerTest::SetUp() {
  main_task_runner_ = new AutoThreadTaskRunner(
      message_loop_.message_loop_proxy(), run_loop_.QuitClosure());
  capture_task_runner_ = main_task_runner_;
  encode_task_runner_ = main_task_runner_;
}

void VideoSchedulerTest::TearDown() {
  // Release the task runners, so that the test can quit.
  capture_task_runner_ = NULL;
  encode_task_runner_ = NULL;
  main_task_runner_ = NULL;

  // Run the MessageLoop until everything has torn down.
  run_loop_.Run();
}

void VideoSchedulerTest::StartVideoScheduler(
    scoped_ptr<webrtc::ScreenCapturer> capturer,
    scoped_ptr<VideoEncoder> encoder) {
  scheduler_ = new VideoScheduler(
      capture_task_runner_,
      encode_task_runner_,
      main_task_runner_,
      capturer.Pass(),
      encoder.Pass(),
      &client_stub_,
      &video_stub_);
  scheduler_->Start();
}

void VideoSchedulerTest::StopVideoScheduler() {
  scheduler_->Stop();
  scheduler_ = NULL;
}

void VideoSchedulerTest::OnCapturerStart(
    webrtc::ScreenCapturer::Callback* callback) {
  EXPECT_FALSE(capturer_callback_);
  EXPECT_TRUE(callback);

  capturer_callback_ = callback;
}

void VideoSchedulerTest::OnCaptureFrame(const webrtc::DesktopRegion& region) {
  frame_->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeXYWH(0, 0, 10, 10));
  capturer_callback_->OnCaptureCompleted(frame_.release());
}

// This test mocks capturer, encoder and network layer to simulate one capture
// cycle. When the first encoded packet is submitted to the network
// VideoScheduler is instructed to come to a complete stop. We expect the stop
// sequence to be executed successfully.
TEST_F(VideoSchedulerTest, StartAndStop) {
  scoped_ptr<webrtc::MockScreenCapturer> capturer(
      new webrtc::MockScreenCapturer());
  Expectation capturer_start =
      EXPECT_CALL(*capturer, Start(_))
          .WillOnce(Invoke(this, &VideoSchedulerTest::OnCapturerStart));

  frame_.reset(new webrtc::BasicDesktopFrame(
      webrtc::DesktopSize(kWidth, kHeight)));

  // First the capturer is called.
  Expectation capturer_capture = EXPECT_CALL(*capturer, Capture(_))
      .After(capturer_start)
      .WillRepeatedly(Invoke(this, &VideoSchedulerTest::OnCaptureFrame));

  scoped_ptr<MockVideoEncoder> encoder(new MockVideoEncoder());

  // Expect the encoder be called.
  EXPECT_CALL(*encoder, EncodePtr(_))
      .WillRepeatedly(FinishEncode());

  // By default delete the arguments when ProcessVideoPacket is received.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillRepeatedly(FinishSend());

  // When the first ProcessVideoPacket is received we stop the VideoScheduler.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          FinishSend(),
          InvokeWithoutArgs(this, &VideoSchedulerTest::StopVideoScheduler)))
      .RetiresOnSaturation();

  // Start video frame capture.
  StartVideoScheduler(capturer.PassAs<webrtc::ScreenCapturer>(),
                      encoder.PassAs<VideoEncoder>());
}

// Verify that the capturer and encoder are torn down on the correct threads.
TEST_F(VideoSchedulerTest, DeleteOnThreads) {
capture_task_runner_ = AutoThread::Create("capture", main_task_runner_);
encode_task_runner_ = AutoThread::Create("encode", main_task_runner_);

  scoped_ptr<webrtc::ScreenCapturer> capturer(
      new ThreadCheckScreenCapturer(capture_task_runner_));
  scoped_ptr<VideoEncoder> encoder(
      new ThreadCheckVideoEncoder(encode_task_runner_));

  // Start and stop the scheduler, so it will tear down the screen capturer and
  // video encoder.
  StartVideoScheduler(capturer.Pass(), encoder.Pass());
  StopVideoScheduler();
}

}  // namespace remoting
