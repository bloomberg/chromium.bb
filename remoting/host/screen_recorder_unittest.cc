// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/task.h"
#include "remoting/base/mock_objects.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/screen_recorder.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::remoting::protocol::MockConnectionToClient;
using ::remoting::protocol::MockVideoStub;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;

namespace remoting {

namespace {

ACTION_P2(RunCallback, rects, data) {
  InvalidRects& dirty_rects = data->mutable_dirty_rects();
  InvalidRects temp_rects;
  std::set_union(dirty_rects.begin(), dirty_rects.end(),
                 rects.begin(), rects.end(),
                 std::inserter(temp_rects, temp_rects.begin()));
  dirty_rects.swap(temp_rects);
  arg0->Run(data);
  delete arg0;
}

ACTION(FinishEncode) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  packet->set_flags(VideoPacket::LAST_PACKET | VideoPacket::LAST_PARTITION);
  arg2->Run(packet.release());
  delete arg2;
}

ACTION(FinishSend) {
  arg1->Run();
  delete arg1;
}

// Helper method to quit the main message loop.
void QuitMessageLoop(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

ACTION_P2(StopScreenRecorder, recorder, task) {
  recorder->Stop(task);
}

}  // namespace

static const int kWidth = 640;
static const int kHeight = 480;
static const media::VideoFrame::Format kFormat = media::VideoFrame::RGB32;
static const VideoPacketFormat::Encoding kEncoding =
    VideoPacketFormat::ENCODING_VERBATIM;

class ScreenRecorderTest : public testing::Test {
 public:
  ScreenRecorderTest() {
  }

  virtual void SetUp() {
    // Capturer and Encoder are owned by ScreenRecorder.
    encoder_ = new MockEncoder();
    connection_ = new MockConnectionToClient();
    record_ = new ScreenRecorder(
        &message_loop_, &message_loop_, &message_loop_,
        &capturer_, encoder_);
  }

 protected:
  scoped_refptr<ScreenRecorder> record_;
  scoped_refptr<MockConnectionToClient> connection_;

  // The following mock objects are owned by ScreenRecorder.
  MockCapturer capturer_;
  MockEncoder* encoder_;
  MessageLoop message_loop_;
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenRecorderTest);
};

// This test mocks capturer, encoder and network layer to operate one recording
// cycle.
TEST_F(ScreenRecorderTest, OneRecordCycle) {
  InvalidRects update_rects;
  update_rects.insert(gfx::Rect(0, 0, 10, 10));
  DataPlanes planes;
  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    planes.data[i] = reinterpret_cast<uint8*>(i);
    planes.strides[i] = kWidth * 4;
  }
  scoped_refptr<CaptureData> data(new CaptureData(planes, kWidth,
                                                  kHeight, kFormat));
  EXPECT_CALL(capturer_, width()).WillRepeatedly(Return(kWidth));
  EXPECT_CALL(capturer_, height()).WillRepeatedly(Return(kHeight));

  // First the capturer is called.
  EXPECT_CALL(capturer_, CaptureInvalidRects(NotNull()))
      .WillOnce(RunCallback(update_rects, data));

  // Expect the encoder be called.
  EXPECT_CALL(*encoder_, Encode(data, false, NotNull()))
      .WillOnce(FinishEncode());

  MockVideoStub video_stub;
  EXPECT_CALL(*connection_, video_stub())
      .WillRepeatedly(Return(&video_stub));

  // Expect the client be notified.
  EXPECT_CALL(video_stub, ProcessVideoPacket(_, _))
      .Times(1)
      .WillOnce(DoAll(DeleteArg<0>(), DeleteArg<1>()));
  EXPECT_CALL(video_stub, GetPendingPackets())
      .Times(AtLeast(0))
      .WillRepeatedly(Return(0));

  // Set the recording rate to very low to avoid capture twice.
  record_->SetMaxRate(0.01);

  // Add the mock client connection to the session.
  record_->AddConnection(connection_);

  // Start the recording.
  record_->Start();

  // Make sure all tasks are completed.
  message_loop_.RunAllPending();
}

// This test mocks capturer, encoder and network layer to simulate one recording
// cycle. When the first encoded packet is submitted to the network
// ScreenRecorder is instructed to come to a complete stop. We expect the stop
// sequence to be executed successfully.
TEST_F(ScreenRecorderTest, StartAndStop) {
  InvalidRects update_rects;
  update_rects.insert(gfx::Rect(0, 0, 10, 10));
  DataPlanes planes;
  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    planes.data[i] = reinterpret_cast<uint8*>(i);
    planes.strides[i] = kWidth * 4;
  }
  scoped_refptr<CaptureData> data(new CaptureData(planes, kWidth,
                                                  kHeight, kFormat));
  EXPECT_CALL(capturer_, width()).WillRepeatedly(Return(kWidth));
  EXPECT_CALL(capturer_, height()).WillRepeatedly(Return(kHeight));

  // First the capturer is called.
  EXPECT_CALL(capturer_, CaptureInvalidRects(NotNull()))
      .WillRepeatedly(RunCallback(update_rects, data));

  // Expect the encoder be called.
  EXPECT_CALL(*encoder_, Encode(data, false, NotNull()))
      .WillRepeatedly(FinishEncode());

  MockVideoStub video_stub;
  EXPECT_CALL(*connection_, video_stub())
      .WillRepeatedly(Return(&video_stub));

  // By default delete the arguments when ProcessVideoPacket is received.
  EXPECT_CALL(video_stub, ProcessVideoPacket(_, _))
      .WillRepeatedly(FinishSend());

  // For the first time when ProcessVideoPacket is received we stop the
  // ScreenRecorder.
  EXPECT_CALL(video_stub, ProcessVideoPacket(_, _))
      .WillOnce(DoAll(
          FinishSend(),
          StopScreenRecorder(record_,
                             NewRunnableFunction(&QuitMessageLoop,
                                                 &message_loop_))))
      .RetiresOnSaturation();

  // Add the mock client connection to the session.
  record_->AddConnection(connection_);

  // Start the recording.
  record_->Start();
  message_loop_.Run();
}

TEST_F(ScreenRecorderTest, StopWithoutStart) {
  record_->Stop(NewRunnableFunction(&QuitMessageLoop, &message_loop_));
  message_loop_.Run();
}

}  // namespace remoting
