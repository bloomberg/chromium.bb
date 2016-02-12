// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_frame_pump.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer_mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace remoting {
namespace protocol {

namespace {

ACTION(FinishSend) {
  arg1.Run();
}

scoped_ptr<webrtc::DesktopFrame> CreateNullFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  return nullptr;
}

scoped_ptr<webrtc::DesktopFrame> CreateUnchangedFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  const webrtc::DesktopSize kSize(800, 640);
  // updated_region() is already empty by default in new BasicDesktopFrames.
  return make_scoped_ptr(new webrtc::BasicDesktopFrame(kSize));
}

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder() {}
  ~MockVideoEncoder() {}

  MOCK_METHOD1(SetLosslessEncode, void(bool));
  MOCK_METHOD1(SetLosslessColor, void(bool));
  MOCK_METHOD1(EncodePtr, VideoPacket*(const webrtc::DesktopFrame&));

  scoped_ptr<VideoPacket> Encode(const webrtc::DesktopFrame& frame) {
    return make_scoped_ptr(EncodePtr(frame));
  }
};

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;

class ThreadCheckVideoEncoder : public VideoEncoderVerbatim {
 public:
  ThreadCheckVideoEncoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
  }
  ~ThreadCheckVideoEncoder() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  scoped_ptr<VideoPacket> Encode(const webrtc::DesktopFrame& frame) override {
    return make_scoped_ptr(new VideoPacket());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckVideoEncoder);
};

class ThreadCheckDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  ThreadCheckDesktopCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner), callback_(nullptr) {}
  ~ThreadCheckDesktopCapturer() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void Start(Callback* callback) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(callback_);
    EXPECT_TRUE(callback);

    callback_ = callback;
  }

  void Capture(const webrtc::DesktopRegion& rect) override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());

    scoped_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(kWidth, kHeight)));
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeXYWH(0, 0, 10, 10));
    callback_->OnCaptureCompleted(frame.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  webrtc::DesktopCapturer::Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCheckDesktopCapturer);
};

class VideoFramePumpTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  void StartVideoFramePump(
      scoped_ptr<webrtc::DesktopCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder);

 protected:
  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
  scoped_refptr<AutoThreadTaskRunner> encode_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  scoped_ptr<VideoFramePump> pump_;

  MockVideoStub video_stub_;
};

void VideoFramePumpTest::SetUp() {
  main_task_runner_ = new AutoThreadTaskRunner(
      message_loop_.task_runner(), run_loop_.QuitClosure());
  encode_task_runner_ = AutoThread::Create("encode", main_task_runner_);
}

void VideoFramePumpTest::TearDown() {
  pump_.reset();

  // Release the task runners, so that the test can quit.
  encode_task_runner_ = nullptr;
  main_task_runner_ = nullptr;

  // Run the MessageLoop until everything has torn down.
  run_loop_.Run();
}

// This test mocks capturer, encoder and network layer to simulate one capture
// cycle.
TEST_F(VideoFramePumpTest, StartAndStop) {
  scoped_ptr<ThreadCheckDesktopCapturer> capturer(
      new ThreadCheckDesktopCapturer(main_task_runner_));
  scoped_ptr<ThreadCheckVideoEncoder> encoder(
      new ThreadCheckVideoEncoder(encode_task_runner_));

  base::RunLoop run_loop;

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_.reset(new VideoFramePump(encode_task_runner_, std::move(capturer),
                                 std::move(encoder), &video_stub_));

  // Run MessageLoop until the first frame is received.
  run_loop.Run();
}

// Tests that the pump handles null frames returned by the capturer.
TEST_F(VideoFramePumpTest, NullFrame) {
  scoped_ptr<FakeDesktopCapturer> capturer(new FakeDesktopCapturer);
  scoped_ptr<MockVideoEncoder> encoder(new MockVideoEncoder);

  base::RunLoop run_loop;

  // Set up the capturer to return null frames.
  capturer->set_frame_generator(base::Bind(&CreateNullFrame));

  // Expect that the VideoEncoder::Encode() method is never called.
  EXPECT_CALL(*encoder, EncodePtr(_)).Times(0);

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_.reset(new VideoFramePump(encode_task_runner_, std::move(capturer),
                                 std::move(encoder), &video_stub_));

  // Run MessageLoop until the first frame is received..
  run_loop.Run();
}

// Tests how the pump handles unchanged frames returned by the capturer.
TEST_F(VideoFramePumpTest, UnchangedFrame) {
  scoped_ptr<FakeDesktopCapturer> capturer(new FakeDesktopCapturer);
  scoped_ptr<MockVideoEncoder> encoder(new MockVideoEncoder);

  base::RunLoop run_loop;

  // Set up the capturer to return unchanged frames.
  capturer->set_frame_generator(base::Bind(&CreateUnchangedFrame));

  // Expect that the VideoEncoder::Encode() method is called.
  EXPECT_CALL(*encoder, EncodePtr(_)).WillRepeatedly(Return(nullptr));

  // When the first ProcessVideoPacket is received we stop the VideoFramePump.
  // TODO(wez): Verify that the generated packet has no content here.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(FinishSend(),
                      InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)))
      .RetiresOnSaturation();

  // Start video frame capture.
  pump_.reset(new VideoFramePump(encode_task_runner_, std::move(capturer),
                                 std::move(encoder), &video_stub_));

  // Run MessageLoop until the first frame is received.
  run_loop.Run();
}

}  // namespace protocol
}  // namespace remoting
