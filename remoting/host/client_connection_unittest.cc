// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "media/base/data_buffer.h"
#include "remoting/base/mock_objects.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/mock_objects.h"
#include "remoting/protocol/fake_connection.h"
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
    connection_ = new FakeChromotingConnection();
    connection_->set_message_loop(&message_loop_);

    // Allocate a ClientConnection object with the mock objects.
    viewer_ = new ClientConnection(&message_loop_, &handler_);
    viewer_->Init(connection_);
    EXPECT_CALL(handler_, OnConnectionOpened(viewer_.get()));
    connection_->get_state_change_callback()->Run(
        ChromotingConnection::CONNECTED);
    message_loop_.RunAllPending();
  }

  MessageLoop message_loop_;
  MockClientConnectionEventHandler handler_;
  scoped_refptr<ClientConnection> viewer_;

  scoped_refptr<FakeChromotingConnection> connection_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientConnectionTest);
};

TEST_F(ClientConnectionTest, SendUpdateStream) {
  // Then send the actual data.
  ChromotingHostMessage message;
  viewer_->SendUpdateStreamPacketMessage(message);

  // And then close the connection to ClientConnection.
  viewer_->Disconnect();

  message_loop_.RunAllPending();

  // Verify that something has been written.
  // TODO(sergeyu): Verify that the correct data has been written.
  EXPECT_GT(connection_->GetVideoChannel()->written_data().size(), 0u);
}

TEST_F(ClientConnectionTest, StateChange) {
  EXPECT_CALL(handler_, OnConnectionClosed(viewer_.get()));
  connection_->get_state_change_callback()->Run(ChromotingConnection::CLOSED);
  message_loop_.RunAllPending();

  EXPECT_CALL(handler_, OnConnectionFailed(viewer_.get()));
  connection_->get_state_change_callback()->Run(ChromotingConnection::FAILED);
  message_loop_.RunAllPending();
}

// Test that we can close client connection more than once and operations
// after the connection is closed has no effect.
TEST_F(ClientConnectionTest, Close) {
  viewer_->Disconnect();
  message_loop_.RunAllPending();
  EXPECT_TRUE(connection_->is_closed());

  ChromotingHostMessage message;
  viewer_->SendUpdateStreamPacketMessage(message);
  viewer_->Disconnect();
  message_loop_.RunAllPending();

  // Verify that nothing has been written.
  EXPECT_EQ(connection_->GetVideoChannel()->written_data().size(), 0u);
}

}  // namespace remoting
