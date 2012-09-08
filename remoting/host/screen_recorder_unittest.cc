// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_recorder.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "remoting/base/capture_data.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::remoting::protocol::MockConnectionToClient;
using ::remoting::protocol::MockConnectionToClientEventHandler;
using ::remoting::protocol::MockHostStub;
using ::remoting::protocol::MockSession;
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

ACTION_P2(RunCallback, region, data) {
  SkRegion& dirty_region = data->mutable_dirty_region();
  dirty_region.op(region, SkRegion::kUnion_Op);
  arg0.Run(data);
}

ACTION(FinishEncode) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  packet->set_flags(VideoPacket::LAST_PACKET | VideoPacket::LAST_PARTITION);
  arg2.Run(packet.Pass());
}

ACTION(FinishSend) {
  arg1.Run();
}

// Helper method to quit the main message loop.
void QuitMessageLoop(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
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

class ScreenRecorderTest : public testing::Test {
 public:
  ScreenRecorderTest() {
  }

  virtual void SetUp() OVERRIDE {
    // VideoFrameCapturer and VideoEncoder are owned by ScreenRecorder.
    encoder_ = new MockVideoEncoder();

    session_ = new MockSession();
    EXPECT_CALL(*session_, SetEventHandler(_));
    EXPECT_CALL(*session_, Close())
        .Times(AnyNumber());
    connection_.reset(new MockConnectionToClient(session_, &host_stub_));
    connection_->SetEventHandler(&handler_);

    record_ = new ScreenRecorder(
        message_loop_.message_loop_proxy(), message_loop_.message_loop_proxy(),
        message_loop_.message_loop_proxy(), &capturer_, encoder_);
  }

  virtual void TearDown() OVERRIDE {
    connection_.reset();
    // Run message loop before destroying because protocol::Session is
    // destroyed asynchronously.
    message_loop_.RunAllPending();
  }

 protected:
  MessageLoop message_loop_;
  scoped_refptr<ScreenRecorder> record_;

  MockConnectionToClientEventHandler handler_;
  MockHostStub host_stub_;
  MockSession* session_;  // Owned by |connection_|.
  scoped_ptr<MockConnectionToClient> connection_;

  // The following mock objects are owned by ScreenRecorder.
  MockVideoFrameCapturer capturer_;
  MockVideoEncoder* encoder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenRecorderTest);
};

// This test mocks capturer, encoder and network layer to simulate one recording
// cycle. When the first encoded packet is submitted to the network
// ScreenRecorder is instructed to come to a complete stop. We expect the stop
// sequence to be executed successfully.
TEST_F(ScreenRecorderTest, StartAndStop) {
  SkRegion update_region(SkIRect::MakeXYWH(0, 0, 10, 10));
  DataPlanes planes;
  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    planes.data[i] = reinterpret_cast<uint8*>(i);
    planes.strides[i] = kWidth * 4;
  }

  Expectation capturer_start = EXPECT_CALL(capturer_, Start(_));

  SkISize size(SkISize::Make(kWidth, kHeight));
  scoped_refptr<CaptureData> data(new CaptureData(planes, size, kFormat));

  EXPECT_CALL(capturer_, size_most_recent())
      .WillRepeatedly(ReturnRef(size));

  EXPECT_CALL(capturer_, InvalidateRegion(_));

  // First the capturer is called.
  Expectation capturer_capture = EXPECT_CALL(capturer_, CaptureInvalidRegion(_))
      .After(capturer_start)
      .WillRepeatedly(RunCallback(update_region, data));

  // Expect the encoder be called.
  EXPECT_CALL(*encoder_, Encode(data, false, _))
      .WillRepeatedly(FinishEncode());

  MockVideoStub video_stub;
  EXPECT_CALL(*connection_, video_stub())
      .WillRepeatedly(Return(&video_stub));

  // By default delete the arguments when ProcessVideoPacket is received.
  EXPECT_CALL(video_stub, ProcessVideoPacketPtr(_, _))
      .WillRepeatedly(FinishSend());

  // For the first time when ProcessVideoPacket is received we stop the
  // ScreenRecorder.
  EXPECT_CALL(video_stub, ProcessVideoPacketPtr(_, _))
      .WillOnce(DoAll(
          FinishSend(),
          StopScreenRecorder(record_,
                             base::Bind(&QuitMessageLoop, &message_loop_))))
      .RetiresOnSaturation();

  EXPECT_CALL(capturer_, Stop())
      .After(capturer_capture);

  // Add the mock client connection to the session.
  record_->AddConnection(connection_.get());

  // Start the recording.
  record_->Start();
  message_loop_.Run();
}

TEST_F(ScreenRecorderTest, StopWithoutStart) {
  EXPECT_CALL(capturer_, Stop());
  record_->Stop(base::Bind(&QuitMessageLoop, &message_loop_));
  message_loop_.Run();
}

}  // namespace remoting
