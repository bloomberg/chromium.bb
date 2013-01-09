// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_scheduler.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "remoting/capturer/capture_data.h"
#include "remoting/capturer/video_capturer_mock_objects.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  packet->set_flags(VideoPacket::LAST_PACKET | VideoPacket::LAST_PARTITION);
  arg2.Run(packet.Pass());
}

ACTION(FinishSend) {
  arg1.Run();
}

ACTION_P2(StopVideoScheduler, scheduler, task) {
  scheduler->get()->Stop(task);
}

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder();
  virtual ~MockVideoEncoder();

  MOCK_METHOD3(Encode, void(
      scoped_refptr<CaptureData> capture_data,
      bool key_frame,
      const DataAvailableCallback& data_available_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoEncoder);
};

MockVideoEncoder::MockVideoEncoder() {}

MockVideoEncoder::~MockVideoEncoder() {}

class VideoSchedulerTest : public testing::Test {
 public:
  VideoSchedulerTest() {
  }

  virtual void SetUp() OVERRIDE {
    encoder_ = new MockVideoEncoder();
  }

  void StartVideoScheduler() {
    scheduler_ = VideoScheduler::Create(
        message_loop_.message_loop_proxy(),
        message_loop_.message_loop_proxy(),
        message_loop_.message_loop_proxy(),
        &capturer_,
        scoped_ptr<VideoEncoder>(encoder_),
        &client_stub_,
        &video_stub_);
  }

  void GenerateOnCaptureCompleted();

 protected:
  MessageLoop message_loop_;
  scoped_refptr<VideoScheduler> scheduler_;

  MockClientStub client_stub_;
  MockVideoStub video_stub_;
  MockVideoFrameCapturer capturer_;

  // The following mock objects are owned by VideoScheduler.
  MockVideoEncoder* encoder_;

  scoped_refptr<CaptureData> data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoSchedulerTest);
};

void VideoSchedulerTest::GenerateOnCaptureCompleted() {
  SkRegion update_region(SkIRect::MakeXYWH(0, 0, 10, 10));
  data_->mutable_dirty_region().op(update_region, SkRegion::kUnion_Op);

  scheduler_->OnCaptureCompleted(data_);
}

// This test mocks capturer, encoder and network layer to simulate one capture
// cycle. When the first encoded packet is submitted to the network
// VideoScheduler is instructed to come to a complete stop. We expect the stop
// sequence to be executed successfully.
TEST_F(VideoSchedulerTest, StartAndStop) {
  Expectation capturer_start = EXPECT_CALL(capturer_, Start(_));

  data_ = new CaptureData(NULL, kWidth * CaptureData::kBytesPerPixel,
                          SkISize::Make(kWidth, kHeight));

  // Create a RunLoop through which to drive |message_loop_|.
  base::RunLoop run_loop;

  // First the capturer is called.
  Expectation capturer_capture = EXPECT_CALL(capturer_, CaptureFrame())
      .After(capturer_start)
      .WillRepeatedly(InvokeWithoutArgs(
          this, &VideoSchedulerTest::GenerateOnCaptureCompleted));

  // Expect the encoder be called.
  EXPECT_CALL(*encoder_, Encode(data_, false, _))
      .WillRepeatedly(FinishEncode());

  // By default delete the arguments when ProcessVideoPacket is received.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillRepeatedly(FinishSend());

  // For the first time when ProcessVideoPacket is received we stop the
  // VideoScheduler.
  EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          FinishSend(),
          StopVideoScheduler(&scheduler_, run_loop.QuitClosure())))
      .RetiresOnSaturation();

  EXPECT_CALL(capturer_, Stop())
      .After(capturer_capture);

  // Start video frame capture.
  StartVideoScheduler();
  run_loop.Run();
}

}  // namespace remoting
