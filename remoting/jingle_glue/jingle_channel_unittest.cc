// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "media/base/data_buffer.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/jingle_glue/mock_objects.h"
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

class MockJingleChannelCallback : public JingleChannel::Callback {
 public:
  MOCK_METHOD2(OnStateChange, void(JingleChannel*, JingleChannel::State));
  MOCK_METHOD2(OnPacketReceived, void(JingleChannel*,
                                      scoped_refptr<media::DataBuffer>));
};

class JingleChannelTest : public testing::Test {
 public:
  virtual ~JingleChannelTest() { }

  // A helper that calls OnStreamEvent(). Need this because we want
  // to call it on the jingle thread.
  static void StreamEvent(JingleChannel* channel,
                          talk_base::StreamInterface* stream,
                          int event, int error,
                          base::WaitableEvent* done_event) {
    channel->OnStreamEvent(stream, event, error);
    if (done_event)
      done_event->Signal();
  }

  static void OnClosed(bool* called) {
    *called = true;
  }

 protected:
  virtual void SetUp() {
    stream_ = new MockStream();  // Freed by the channel.
    channel_ = new JingleChannel(&callback_);
    channel_->thread_ = &thread_;
    channel_->stream_.reset(stream_);
  }

  JingleThread thread_;
  MockStream* stream_;
  MockJingleChannelCallback callback_;
  scoped_refptr<JingleChannel> channel_;
};

TEST_F(JingleChannelTest, Init) {
  EXPECT_CALL(*stream_, GetState())
      .Times(1)
      .WillRepeatedly(Return(talk_base::SS_OPENING));

  EXPECT_CALL(callback_, OnStateChange(channel_.get(),
                                       JingleChannel::CONNECTING))
      .Times(1);

  thread_.Start();

  EXPECT_EQ(JingleChannel::INITIALIZING, channel_->state());
  channel_->Init(&thread_, stream_, "user@domain.com");
  EXPECT_EQ(JingleChannel::CONNECTING, channel_->state());
  channel_->closed_ = true;

  thread_.Stop();
}

TEST_F(JingleChannelTest, Write) {
  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(kBufferSize);
  data->SetDataSize(kBufferSize);

  EXPECT_CALL(*stream_, Write(static_cast<const void*>(data->GetData()),
                             kBufferSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kBufferSize),
                      Return(talk_base::SR_SUCCESS)));

  channel_->state_ = JingleChannel::OPEN;
  thread_.Start();
  channel_->Write(data);
  thread_.Stop();
  channel_->closed_ = true;
}

TEST_F(JingleChannelTest, Read) {
  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(kBufferSize);
  data->SetDataSize(kBufferSize);

  EXPECT_CALL(callback_, OnPacketReceived(channel_.get(), _))
      .Times(1);

  EXPECT_CALL(*stream_, GetAvailable(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(kBufferSize),
                      Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<0>(0),
                      Return(true)));

  EXPECT_CALL(*stream_, Read(_, kBufferSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kBufferSize),
                      Return(talk_base::SR_SUCCESS)));

  channel_->state_ = JingleChannel::OPEN;
  thread_.Start();

  base::WaitableEvent done_event(true, false);
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &JingleChannelTest::StreamEvent, channel_.get(), stream_,
      talk_base::SE_READ, 0, &done_event));
  done_event.Wait();

  thread_.Stop();

  channel_->closed_ = true;
}

TEST_F(JingleChannelTest, Close) {
  EXPECT_CALL(*stream_, Close()).Times(1);
  // Don't expect any calls except Close().
  EXPECT_CALL(*stream_, GetAvailable(_)).Times(0);
  EXPECT_CALL(*stream_, Read(_, _, _, _)).Times(0);
  EXPECT_CALL(callback_, OnPacketReceived(_, _)).Times(0);

  thread_.Start();
  channel_->Close();
  // Verify that the channel doesn't call callback anymore.
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableFunction(
      &JingleChannelTest::StreamEvent, channel_.get(), stream_,
      talk_base::SE_READ, 0, static_cast<base::WaitableEvent*>(NULL)));
  thread_.Stop();
}

TEST_F(JingleChannelTest, ClosedTask) {
  EXPECT_CALL(*stream_, Close())
      .Times(1);

  thread_.Start();
  bool closed = false;
  channel_->Close(NewRunnableFunction(&JingleChannelTest::OnClosed,
                                      &closed));
  thread_.Stop();
  EXPECT_TRUE(closed);
}

TEST_F(JingleChannelTest, DoubleClose) {
  EXPECT_CALL(*stream_, Close())
      .Times(1);

  thread_.Start();
  bool closed1 = false;
  channel_->Close(NewRunnableFunction(&JingleChannelTest::OnClosed,
                                     &closed1));
  bool closed2 = false;
  channel_->Close(NewRunnableFunction(&JingleChannelTest::OnClosed,
                                     &closed2));
  thread_.Stop();
  EXPECT_TRUE(closed1 && closed2);
}

}  // namespace remoting
