// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/base_mock_objects.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace remoting {
namespace protocol {

class ConnectionToClientTest : public testing::Test {
 public:
  ConnectionToClientTest() {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    session_ = new protocol::FakeSession();
    session_->set_message_loop(&message_loop_);

    // Allocate a ClientConnection object with the mock objects.
    viewer_.reset(new ConnectionToClient(session_));
    viewer_->set_host_stub(&host_stub_);
    viewer_->set_input_stub(&input_stub_);
    viewer_->SetEventHandler(&handler_);
    EXPECT_CALL(handler_, OnConnectionOpened(viewer_.get()));
    session_->state_change_callback().Run(
        protocol::Session::CONNECTED);
    session_->state_change_callback().Run(
        protocol::Session::AUTHENTICATED);
    message_loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    viewer_.reset();
    message_loop_.RunAllPending();
  }

  MessageLoop message_loop_;
  MockConnectionToClientEventHandler handler_;
  MockHostStub host_stub_;
  MockInputStub input_stub_;
  scoped_ptr<ConnectionToClient> viewer_;

  // Owned by |viewer_|.
  protocol::FakeSession* session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToClientTest);
};

TEST_F(ConnectionToClientTest, SendUpdateStream) {
  // Then send the actual data.
  VideoPacket* packet = new VideoPacket();
  viewer_->video_stub()->ProcessVideoPacket(
      packet, base::Bind(&DeletePointer<VideoPacket>, packet));

  message_loop_.RunAllPending();

  // Verify that something has been written.
  // TODO(sergeyu): Verify that the correct data has been written.
  ASSERT_TRUE(session_->GetStreamChannel(kVideoChannelName));
  EXPECT_GT(session_->GetStreamChannel(kVideoChannelName)->
            written_data().size(), 0u);

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect();

  message_loop_.RunAllPending();
}

TEST_F(ConnectionToClientTest, NoWriteAfterDisconnect) {
  // Then send the actual data.
  VideoPacket* packet = new VideoPacket();
  viewer_->video_stub()->ProcessVideoPacket(
      packet, base::Bind(&DeletePointer<VideoPacket>, packet));

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect();

  // The test will crash if data writer tries to write data to the
  // channel socket.
  // TODO(sergeyu): Use MockSession to verify that no data is written?
  message_loop_.RunAllPending();
}

TEST_F(ConnectionToClientTest, StateChange) {
  EXPECT_CALL(handler_, OnConnectionClosed(viewer_.get()));
  session_->state_change_callback().Run(protocol::Session::CLOSED);
  message_loop_.RunAllPending();

  EXPECT_CALL(handler_, OnConnectionFailed(
      viewer_.get(), Session::SESSION_REJECTED));
  session_->set_error(Session::SESSION_REJECTED);
  session_->state_change_callback().Run(protocol::Session::FAILED);
  message_loop_.RunAllPending();
}

}  // namespace protocol
}  // namespace remoting
