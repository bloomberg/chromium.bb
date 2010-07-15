// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "media/base/data_buffer.h"
#include "remoting/base/mock_objects.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/mock_objects.h"
#include "remoting/jingle_glue/mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace remoting {

class ClientConnectionTest : public testing::Test {
 public:
  ClientConnectionTest() {
  }

 protected:
  virtual void SetUp() {
    decoder_ = new MockProtocolDecoder();
    channel_ = new StrictMock<MockJingleChannel>();

    // Allocate a ClientConnection object with the mock objects. we give the
    // ownership of decoder to the viewer.
    viewer_ = new ClientConnection(&message_loop_,
                                   decoder_,
                                   &handler_);

    viewer_->set_jingle_channel(channel_.get());
  }

  MessageLoop message_loop_;
  MockProtocolDecoder* decoder_;
  MockClientConnectionEventHandler handler_;
  scoped_refptr<ClientConnection> viewer_;

  // |channel_| is wrapped with StrictMock because we limit strictly the calls
  // to it.
  scoped_refptr<StrictMock<MockJingleChannel> > channel_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientConnectionTest);
};

TEST_F(ClientConnectionTest, SendUpdateStream) {
  // Tell the viewer we are starting an update stream.
  EXPECT_CALL(*channel_, Write(_));
  viewer_->SendBeginUpdateStreamMessage();

  // Then send the actual data.
  EXPECT_CALL(*channel_, Write(_));
  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(10);
  viewer_->SendUpdateStreamPacketMessage(data);

  // Send the end of update message.
  EXPECT_CALL(*channel_, Write(_));
  viewer_->SendEndUpdateStreamMessage();

  // And then close the connection to ClientConnection.
  EXPECT_CALL(*channel_, Close());
  viewer_->Disconnect();
}

TEST_F(ClientConnectionTest, StateChange) {
  EXPECT_CALL(handler_, OnConnectionOpened(viewer_.get()));
  viewer_->OnStateChange(channel_.get(), JingleChannel::OPEN);
  message_loop_.RunAllPending();

  EXPECT_CALL(handler_, OnConnectionClosed(viewer_.get()));
  viewer_->OnStateChange(channel_.get(), JingleChannel::CLOSED);
  message_loop_.RunAllPending();

  EXPECT_CALL(handler_, OnConnectionFailed(viewer_.get()));
  viewer_->OnStateChange(channel_.get(), JingleChannel::FAILED);
  message_loop_.RunAllPending();
}

TEST_F(ClientConnectionTest, ParseMessages) {
  scoped_refptr<media::DataBuffer> data;

  // Give the data to the ClientConnection, it will use ProtocolDecoder to
  // decode the messages.
  EXPECT_CALL(*decoder_, ParseClientMessages(data, NotNull()));
  EXPECT_CALL(handler_, HandleMessages(viewer_.get(), NotNull()));

  viewer_->OnPacketReceived(channel_.get(), data);
  message_loop_.RunAllPending();
}

// Test that we can close client connection more than once and operations
// after the connection is closed has no effect.
TEST_F(ClientConnectionTest, Close) {
  EXPECT_CALL(*channel_, Close());
  viewer_->Disconnect();

  scoped_refptr<media::DataBuffer> data = new media::DataBuffer(10);
  viewer_->SendUpdateStreamPacketMessage(data);
  viewer_->SendEndUpdateStreamMessage();
  viewer_->Disconnect();
}

}  // namespace remoting
