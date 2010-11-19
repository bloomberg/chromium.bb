// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/task.h"
#include "remoting/base/mock_objects.h"
#include "remoting/host/mock_objects.h"
#include "remoting/host/session_manager.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::remoting::protocol::MockConnectionToClient;
using ::remoting::protocol::MockVideoStub;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NotNull;
using ::testing::Return;

namespace remoting {

static const int kWidth = 640;
static const int kHeight = 480;
static const media::VideoFrame::Format kFormat = media::VideoFrame::RGB32;
static const VideoPacketFormat::Encoding kEncoding =
    VideoPacketFormat::ENCODING_VERBATIM;

class SessionManagerTest : public testing::Test {
 public:
  SessionManagerTest() {
  }

 protected:
  void Init() {
    capturer_ = new MockCapturer();
    encoder_ = new MockEncoder();
    connection_ = new MockConnectionToClient();
    record_ = new SessionManager(
        &message_loop_, &message_loop_, &message_loop_,
        capturer_, encoder_);
  }

  scoped_refptr<SessionManager> record_;
  scoped_refptr<MockConnectionToClient> connection_;
  MockCapturer* capturer_;
  MockEncoder* encoder_;
  MessageLoop message_loop_;
 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerTest);
};

TEST_F(SessionManagerTest, Init) {
  Init();
}

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

ACTION_P(FinishEncode, msg) {
  arg2->Run(msg);
  delete arg2;
}

// BUG 57351
TEST_F(SessionManagerTest, DISABLED_OneRecordCycle) {
  Init();

  InvalidRects update_rects;
  update_rects.insert(gfx::Rect(0, 0, 10, 10));
  DataPlanes planes;
  for (int i = 0; i < DataPlanes::kPlaneCount; ++i) {
    planes.data[i] = reinterpret_cast<uint8*>(i);
    planes.strides[i] = kWidth * 4;
  }
  scoped_refptr<CaptureData> data(new CaptureData(planes, kWidth,
                                                  kHeight, kFormat));
  // Set the recording rate to very low to avoid capture twice.
  record_->SetMaxRate(0.01);

  // Add the mock client connection to the session.
  EXPECT_CALL(*capturer_, width()).WillRepeatedly(Return(kWidth));
  EXPECT_CALL(*capturer_, height()).WillRepeatedly(Return(kHeight));
  record_->AddConnection(connection_);

  // First the capturer is called.
  EXPECT_CALL(*capturer_, CaptureInvalidRects(NotNull()))
      .WillOnce(RunCallback(update_rects, data));

  // Expect the encoder be called.
  VideoPacket* packet = new VideoPacket();
  EXPECT_CALL(*encoder_, Encode(data, false, NotNull()))
      .WillOnce(FinishEncode(packet));

  MockVideoStub video_stub;
  EXPECT_CALL(*connection_, video_stub())
      .WillRepeatedly(Return(&video_stub));

  // Expect the client be notified.
  EXPECT_CALL(video_stub, ProcessVideoPacket(_, _));
  EXPECT_CALL(video_stub, GetPendingPackets())
      .Times(AtLeast(0))
      .WillRepeatedly(Return(0));

  // Start the recording.
  record_->Start();

  // Make sure all tasks are completed.
  message_loop_.RunAllPending();
}

// TODO(hclam): Add test for double buffering.
// TODO(hclam): Add test for multiple captures.
// TODO(hclam): Add test for interruption.

}  // namespace remoting
