// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_connection_to_client.h"

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

class IpcConnectionToClientTest : public testing::Test {
 public:
  IpcConnectionToClientTest() {}

 protected:
  void SetUp() override {
    session_ = new FakeSession();

    // Allocate a ClientConnection object with the mock objects.
    viewer_.reset(new IceConnectionToClient(make_scoped_ptr(session_),
                                            message_loop_.task_runner()));
    viewer_->SetEventHandler(&handler_);
    EXPECT_CALL(handler_, OnConnectionAuthenticated(viewer_.get()))
        .WillOnce(
            InvokeWithoutArgs(this, &IpcConnectionToClientTest::ConnectStubs));
    EXPECT_CALL(handler_, OnConnectionChannelsConnected(viewer_.get()));
    session_->event_handler()->OnSessionStateChange(Session::ACCEPTED);
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

  FakeSession* session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IpcConnectionToClientTest);
};

TEST_F(IpcConnectionToClientTest, SendUpdateStream) {
  Capabilities capabilities;
  viewer_->client_stub()->SetCapabilities(capabilities);

  base::RunLoop().RunUntilIdle();

  // Verify that something has been written.
  // TODO(sergeyu): Verify that the correct data has been written.
  FakeStreamSocket* channel =
      session_->GetTransport()->GetStreamChannelFactory()->GetFakeChannel(
          kControlChannelName);
  ASSERT_TRUE(channel);
  EXPECT_FALSE(channel->written_data().empty());

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect(protocol::OK);

  base::RunLoop().RunUntilIdle();
}

TEST_F(IpcConnectionToClientTest, NoWriteAfterDisconnect) {
  Capabilities capabilities;
  viewer_->client_stub()->SetCapabilities(capabilities);

  // And then close the connection to ConnectionToClient.
  viewer_->Disconnect(protocol::OK);

  // The test will crash if data writer tries to write data to the
  // channel socket.
  // TODO(sergeyu): Use MockSession to verify that no data is written?
  base::RunLoop().RunUntilIdle();
}

TEST_F(IpcConnectionToClientTest, StateChange) {
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
