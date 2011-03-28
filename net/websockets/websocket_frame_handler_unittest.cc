// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_frame_handler.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

TEST(WebSocketFrameHandlerTest, Basic) {
  const char kInputData[] = "\0hello, world\xff";
  const int kInputDataLen = sizeof(kInputData) - 1;  // no terminating NUL.

  scoped_ptr<WebSocketFrameHandler> handler(new WebSocketFrameHandler);

  // No data.
  EXPECT_EQ(0, handler->UpdateCurrentBuffer(true));
  EXPECT_TRUE(handler->GetCurrentBuffer() == NULL);
  EXPECT_EQ(0, handler->GetCurrentBufferSize());

  // WebKit sends data (WebSocketJob::SendData),
  // or data is received from network (WebSocketJob::OnReceivedData)
  handler->AppendData(kInputData, kInputDataLen);
  EXPECT_TRUE(handler->GetCurrentBuffer() == NULL);
  EXPECT_GT(handler->UpdateCurrentBuffer(true), 0);
  // Get data to send to the socket (send),
  // or to send to WebKit (receive).
  IOBuffer* buf = handler->GetCurrentBuffer();
  ASSERT_TRUE(buf != NULL);
  EXPECT_TRUE(memcmp(buf->data(), kInputData, kInputDataLen) == 0);
  EXPECT_EQ(kInputDataLen, handler->GetCurrentBufferSize());
  EXPECT_EQ(kInputDataLen, handler->GetOriginalBufferSize());
  // Data was sent. (WebSocketJob::OnSentData)
  buf = NULL;
  handler->ReleaseCurrentBuffer();
  EXPECT_TRUE(handler->GetCurrentBuffer() == NULL);
  EXPECT_EQ(0, handler->GetCurrentBufferSize());
  EXPECT_EQ(0, handler->UpdateCurrentBuffer(true));
}

TEST(WebSocketFrameHandlerTest, ParseFrame) {
  std::vector<WebSocketFrameHandler::FrameInfo> frames;
  const char kInputData[] = "\0hello, world\xff\xff\0";
  const int kInputDataLen = sizeof(kInputData) - 1;
  const int kHelloWorldFrameLen = 14;

  EXPECT_EQ(kInputDataLen,
            WebSocketFrameHandler::ParseWebSocketFrame(
                kInputData, kInputDataLen, &frames));
  EXPECT_EQ(2UL, frames.size());

  EXPECT_EQ(kInputData, frames[0].frame_start);
  EXPECT_EQ(kHelloWorldFrameLen, frames[0].frame_length);
  EXPECT_EQ(kInputData + 1, frames[0].message_start);
  EXPECT_EQ(kHelloWorldFrameLen - 2, frames[0].message_length);

  EXPECT_EQ(kInputData + kHelloWorldFrameLen, frames[1].frame_start);
  EXPECT_EQ(2, frames[1].frame_length);
  EXPECT_EQ(0, frames[1].message_length);
}

TEST(WebSocketFrameHandlerTest, ParseFrameLength) {
  std::vector<WebSocketFrameHandler::FrameInfo> frames;
  const char kHelloWorldFrame[] = "\0hello, world\xff";
  const int kHelloWorldFrameLen = sizeof(kHelloWorldFrame) - 1;
  const char kLengthFrame[3 + 129] = "\x80\x81\x01\x01\0should be skipped\xff";
  const int kLengthFrameLen = sizeof(kLengthFrame);
  const int kInputDataLen = kHelloWorldFrameLen +
      kLengthFrameLen +
      kHelloWorldFrameLen;
  char inputData[kInputDataLen];
  memcpy(inputData, kHelloWorldFrame, kHelloWorldFrameLen);
  memcpy(inputData + kHelloWorldFrameLen, kLengthFrame, kLengthFrameLen);
  memcpy(inputData + kHelloWorldFrameLen + kLengthFrameLen,
         kHelloWorldFrame, kHelloWorldFrameLen);

  EXPECT_EQ(kInputDataLen,
            WebSocketFrameHandler::ParseWebSocketFrame(
                inputData, kInputDataLen, &frames));
  ASSERT_EQ(3UL, frames.size());

  EXPECT_EQ(inputData, frames[0].frame_start);
  EXPECT_EQ(kHelloWorldFrameLen, frames[0].frame_length);
  EXPECT_EQ(inputData + 1, frames[0].message_start);
  EXPECT_EQ(kHelloWorldFrameLen - 2, frames[0].message_length);

  EXPECT_EQ(inputData + kHelloWorldFrameLen, frames[1].frame_start);
  EXPECT_EQ(kLengthFrameLen, frames[1].frame_length);
  EXPECT_EQ(inputData + kHelloWorldFrameLen + 3, frames[1].message_start);
  EXPECT_EQ(kLengthFrameLen - 3, frames[1].message_length);

  EXPECT_EQ(inputData + kHelloWorldFrameLen + kLengthFrameLen,
            frames[2].frame_start);
  EXPECT_EQ(kHelloWorldFrameLen, frames[2].frame_length);
  EXPECT_EQ(inputData + kHelloWorldFrameLen + kLengthFrameLen + 1,
            frames[2].message_start);
  EXPECT_EQ(kHelloWorldFrameLen - 2, frames[2].message_length);
}

TEST(WebSocketFrameHandlerTest, ParsePartialFrame) {
  std::vector<WebSocketFrameHandler::FrameInfo> frames;
  const char kInputData[] = "\0hello, world\xff"
      "\x80\x81\x01"  // skip 1*128+1 bytes.
      "\x01\xff"
      "\0should be skipped\xff";
  const int kInputDataLen = sizeof(kInputData) - 1;
  const int kHelloWorldFrameLen = 14;

  EXPECT_EQ(kHelloWorldFrameLen,
            WebSocketFrameHandler::ParseWebSocketFrame(
                kInputData, kInputDataLen, &frames));
  ASSERT_EQ(1UL, frames.size());

  EXPECT_EQ(kInputData, frames[0].frame_start);
  EXPECT_EQ(kHelloWorldFrameLen, frames[0].frame_length);
  EXPECT_EQ(kInputData + 1, frames[0].message_start);
  EXPECT_EQ(kHelloWorldFrameLen - 2, frames[0].message_length);
}

}  // namespace net
