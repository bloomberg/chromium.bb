// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "media/base/data_buffer.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/base/stream.h"

using testing::_;
using testing::Return;
using testing::Mock;
using testing::SetArgumentPointee;

namespace remoting {

namespace {
// Size of test buffer in bytes.
const size_t kBufferSize = 100;
}  // namespace

class MockCallback : public JingleChannel::Callback {
 public:
  MOCK_METHOD2(OnStateChange, void(JingleChannel*, JingleChannel::State));
  MOCK_METHOD2(OnPacketReceived, void(JingleChannel*,
                                      scoped_refptr<media::DataBuffer>));
};

class MockStream : public talk_base::StreamInterface {
 public:
  virtual ~MockStream() {}
  MOCK_CONST_METHOD0(GetState, talk_base::StreamState());

  MOCK_METHOD4(Read, talk_base::StreamResult(void*, size_t, size_t*, int*));
  MOCK_METHOD4(Write, talk_base::StreamResult(const void*, size_t,
                                              size_t*, int*));
  MOCK_CONST_METHOD1(GetAvailable, bool(size_t*));
  MOCK_METHOD0(Close, void());

  MOCK_METHOD3(PostEvent, void(talk_base::Thread*, int, int));
  MOCK_METHOD2(PostEvent, void(int, int));
};

TEST(JingleChannelTest, Init) {
  JingleThread thread;

  MockStream* stream = new MockStream();
  MockCallback callback;

  EXPECT_CALL(*stream, GetState())
      .Times(1)
      .WillRepeatedly(Return(talk_base::SS_OPENING));

  scoped_refptr<JingleChannel> channel = new JingleChannel(&callback);

  EXPECT_CALL(callback, OnStateChange(channel.get(), JingleChannel::CONNECTING))
      .Times(1);

  thread.Start();

  EXPECT_EQ(JingleChannel::INITIALIZING, channel->state());
  channel->Init(&thread, stream, "user@domain.com");
  EXPECT_EQ(JingleChannel::CONNECTING, channel->state());
  channel->state_ = JingleChannel::CLOSED;

  thread.Stop();
}

TEST(JingleChannelTest, Write) {
  JingleThread thread;
  MockStream* stream = new MockStream();  // Freed by the channel.
  MockCallback callback;

  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(kBufferSize);
  data->SetDataSize(kBufferSize);

  EXPECT_CALL(*stream, Write(static_cast<const void*>(data->GetData()),
                             kBufferSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kBufferSize),
                      Return(talk_base::SR_SUCCESS)));

  scoped_refptr<JingleChannel> channel = new JingleChannel(&callback);

  channel->thread_ = &thread;
  channel->stream_.reset(stream);
  channel->state_ = JingleChannel::OPEN;
  thread.Start();
  channel->Write(data);
  thread.Stop();
  channel->state_ = JingleChannel::CLOSED;
}

TEST(JingleChannelTest, Read) {
  JingleThread thread;
  MockStream* stream = new MockStream();  // Freed by the channel.
  MockCallback callback;

  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(kBufferSize);
  data->SetDataSize(kBufferSize);

  scoped_refptr<JingleChannel> channel = new JingleChannel(&callback);

  EXPECT_CALL(callback, OnPacketReceived(channel.get(), _))
      .Times(1);

  EXPECT_CALL(*stream, GetAvailable(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(kBufferSize),
                      Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<0>(0),
                      Return(true)));

  EXPECT_CALL(*stream, Read(_, kBufferSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kBufferSize),
                      Return(talk_base::SR_SUCCESS)));

  channel->thread_ = &thread;
  channel->stream_.reset(stream);
  channel->state_ = JingleChannel::OPEN;
  thread.Start();
  channel->OnStreamEvent(stream, talk_base::SE_READ, 0);
  thread.Stop();
  channel->state_ = JingleChannel::CLOSED;
}

TEST(JingleChannelTest, Close) {
  JingleThread thread;
  MockStream* stream = new MockStream();  // Freed by the channel.
  MockCallback callback;

  EXPECT_CALL(*stream, Close())
      .Times(1);

  scoped_refptr<JingleChannel> channel = new JingleChannel(&callback);

  channel->thread_ = &thread;
  channel->stream_.reset(stream);
  channel->state_ = JingleChannel::OPEN;

  EXPECT_CALL(callback, OnStateChange(channel.get(), JingleChannel::CLOSED))
      .Times(1);

  thread.Start();
  channel->Close();
  thread.Stop();
}

}  // namespace remoting
