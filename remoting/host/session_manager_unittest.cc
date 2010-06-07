// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/task.h"
#include "remoting/host/mock_objects.h"
#include "remoting/host/session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NotNull;
using ::testing::Return;

namespace remoting {

static const int kWidth = 640;
static const int kHeight = 480;
static int kStride[3] = {
  kWidth * 4,
  kWidth * 4,
  kWidth * 4,
};
static uint8* kData[3] = {
  reinterpret_cast<uint8*>(0x01),
  reinterpret_cast<uint8*>(0x02),
  reinterpret_cast<uint8*>(0x03),
};
static const chromotocol_pb::PixelFormat kFormat =
    chromotocol_pb::PixelFormatRgb32;

class SessionManagerTest : public testing::Test {
 public:
  SessionManagerTest() {
  }

 protected:
  void Init() {
    capturer_ = new MockCapturer();
    encoder_ = new MockEncoder();
    client_ = new MockClientConnection();
    record_ = new SessionManager(&message_loop_,
                                &message_loop_,
                                &message_loop_,
                                capturer_,
                                encoder_);
  }

  scoped_refptr<SessionManager> record_;
  scoped_refptr<MockClientConnection> client_;
  MockCapturer* capturer_;
  MockEncoder* encoder_;
  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManagerTest);
};

TEST_F(SessionManagerTest, Init) {
  Init();
}

ACTION(RunSimpleTask) {
  arg0->Run();
  delete arg0;
}

ACTION_P2(FinishDecode, header, data) {
  *arg4 = header;
  *arg5 = data;
  *arg6 = true;
  arg7->Run();
  delete arg7;
}

ACTION_P(AssignCaptureData, data) {
  arg0[0] = data[0];
  arg0[1] = data[1];
  arg0[2] = data[2];
}

TEST_F(SessionManagerTest, OneRecordCycle) {
  Init();

  // Set the recording rate to very low to avoid capture twice.
  record_->SetMaxRate(0.01);

  // Add the mock client connection to the session.
  EXPECT_CALL(*capturer_, GetWidth()).WillRepeatedly(Return(kWidth));
  EXPECT_CALL(*capturer_, GetHeight()).WillRepeatedly(Return(kHeight));
  EXPECT_CALL(*client_, SendInitClientMessage(kWidth, kHeight));
  record_->AddClient(client_);

  // First the capturer is called.
  EXPECT_CALL(*capturer_, CaptureDirtyRects(NotNull()))
      .WillOnce(RunSimpleTask());
  // TODO(hclam): Return DirtyRects for verification.
  EXPECT_CALL(*capturer_, GetDirtyRects(NotNull()));
  EXPECT_CALL(*capturer_, GetData(NotNull()))
      .WillOnce(AssignCaptureData(kData));
  EXPECT_CALL(*capturer_, GetDataStride(NotNull()))
      .WillOnce(AssignCaptureData(kStride));
  EXPECT_CALL(*capturer_, GetPixelFormat())
      .WillOnce(Return(kFormat));

  // Expect the encoder be called.
  chromotocol_pb::UpdateStreamPacketHeader header;
  scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(0);
  EXPECT_CALL(*encoder_, SetSize(kWidth, kHeight));
  EXPECT_CALL(*encoder_, SetPixelFormat(kFormat));
  // TODO(hclam): Expect the content of the dirty rects.
  EXPECT_CALL(*encoder_,
              Encode(_, NotNull(), NotNull(), false, NotNull(),
                     NotNull(), NotNull(), NotNull()))
      .WillOnce(FinishDecode(header, buffer));

  // Expect the client be notified.
  EXPECT_CALL(*client_, SendBeginUpdateStreamMessage());
  EXPECT_CALL(*client_, SendUpdateStreamPacketMessage(NotNull(), buffer));
  EXPECT_CALL(*client_, SendEndUpdateStreamMessage());
  EXPECT_CALL(*client_, GetPendingUpdateStreamMessages())
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
