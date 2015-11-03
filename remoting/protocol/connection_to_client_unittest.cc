// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace remoting {
namespace protocol {

class ConnectionToClientTest : public testing::Test {
 public:
  ConnectionToClientTest() {
  }

 protected:
  void SetUp() override {
    session_ = new FakeSession();

    // Allocate a ClientConnection object with the mock objects.
    viewer_.reset(new ConnectionToClient(session_));
    viewer_->SetEventHandler(&handler_);
    EXPECT_CALL(handler_, OnConnectionAuthenticated(viewer_.get()))
        .WillOnce(
            InvokeWithoutArgs(this, &ConnectionToClientTest::ConnectStubs));
    EXPECT_CALL(handler_, OnConnectionChannelsConnected(viewer_.get()));
    session_->event_handler()->OnSessionStateChange(Session::CONNECTED);
    session_->event_handler()->OnSessionStateChange(Session::AUTHENTICATED);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    viewer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ConnectStubs() {
    viewer_->set_clipboard_stub(&clipboard_stub_);
    viewer_->set_host_stub(&host_stub_);
    viewer_->set_input_stub(&input_stub_);
  }

  base::MessageLoop message_loop_;
  MockConnectionToClientEventHandler handler_;
  MockClipboardStub clipboard_stub_;
  MockHostStub host_stub_;
  MockInputStub input_stub_;
  scoped_ptr<ConnectionToClient> viewer_;

  // Owned by |viewer_|.
  FakeSession* session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionToClientTest);
};

TEST_F(ConnectionToClientTest, SendUpdateStream) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  viewer_->video_stub()->ProcessVideoPacket(packet.Pass(), base::Closure());

  base::RunLoop().RunUntilIdle();

  // Verify that something has been written.
  // TODO(sergeyu): Verify that the correct data has been written.
  FakeStreamSocket* channel =
      session_->GetTransport()->GetStreamChannelFactory()->GetFakeChannel(
          kVideoChannelName);
  ASSERT_TRUE(channel);
  EXPECT_FALSE(channel->written_data().empty());

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect(protocol::OK);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ConnectionToClientTest, NoWriteAfterDisconnect) {
  scoped_ptr<VideoPacket> packet(new VideoPacket());
  viewer_->video_stub()->ProcessVideoPacket(packet.Pass(), base::Closure());

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect(protocol::OK);

  // The test will crash if data writer tries to write data to the
  // channel socket.
  // TODO(sergeyu): Use MockSession to verify that no data is written?
  base::RunLoop().RunUntilIdle();
}

TEST_F(ConnectionToClientTest, StateChange) {
  EXPECT_CALL(handler_, OnConnectionClosed(viewer_.get(), OK));
  session_->event_handler()->OnSessionStateChange(Session::CLOSED);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(handler_, OnConnectionClosed(viewer_.get(), SESSION_REJECTED));
  session_->set_error(SESSION_REJECTED);
  session_->event_handler()->OnSessionStateChange(Session::FAILED);
  base::RunLoop().RunUntilIdle();
}

}  // namespace protocol
}  // namespace remoting
