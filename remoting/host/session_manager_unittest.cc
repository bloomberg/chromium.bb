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
static const PixelFormat kFormat = PixelFormatRgb32;
static const UpdateStreamEncoding kEncoding = EncodingNone;

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

ACTION_P3(FinishDecode, rects, buffer, header) {
  gfx::Rect& rect = (*rects)[0];
  Encoder::EncodingState state = (Encoder::EncodingStarting |
                                  Encoder::EncodingInProgress |
                                  Encoder::EncodingEnded);
  header->set_x(rect.x());
  header->set_y(rect.y());
  header->set_width(rect.width());
  header->set_height(rect.height());
  header->set_encoding(kEncoding);
  header->set_pixel_format(kFormat);
  arg4->Run(header, *buffer, state);
}

ACTION_P(AssignCaptureData, data) {
  arg0[0] = data[0];
  arg0[1] = data[1];
  arg0[2] = data[2];
}

ACTION_P(AssignDirtyRect, rects) {
  *arg0 = *rects;
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

  // Create a dirty rect that can be verified.
  DirtyRects rects;
  rects.push_back(gfx::Rect(0, 0, 10, 10));
  EXPECT_CALL(*capturer_, GetDirtyRects(NotNull()))
      .WillOnce(AssignDirtyRect(&rects));
  EXPECT_CALL(*capturer_, GetData(NotNull()))
      .WillOnce(AssignCaptureData(kData));
  EXPECT_CALL(*capturer_, GetDataStride(NotNull()))
      .WillOnce(AssignCaptureData(kStride));
  EXPECT_CALL(*capturer_, GetPixelFormat())
      .WillOnce(Return(kFormat));

  // Expect the encoder be called.
  scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(0);
  EXPECT_CALL(*encoder_, SetSize(kWidth, kHeight));
  EXPECT_CALL(*encoder_, SetPixelFormat(kFormat));
  UpdateStreamPacketHeader *header = new UpdateStreamPacketHeader;
  EXPECT_CALL(*encoder_, Encode(rects, NotNull(), NotNull(), false, NotNull()))
      .WillOnce(FinishDecode(&rects, &buffer, header));

  // Expect the client be notified.
  EXPECT_CALL(*client_, SendBeginUpdateStreamMessage());

  EXPECT_CALL(*client_, SendUpdateStreamPacketMessage(header ,buffer));
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
