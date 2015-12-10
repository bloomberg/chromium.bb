// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_connection_to_client.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class WebrtcConnectionToClientTest : public testing::Test {
 public:
  WebrtcConnectionToClientTest() {}

 protected:
  void SetUp() override {
    session_ = new FakeSession();

    // Allocate a ClientConnection object with the mock objects.
    connection_.reset(new WebrtcConnectionToClient(make_scoped_ptr(session_)));
    connection_->SetEventHandler(&handler_);
    EXPECT_CALL(handler_, OnConnectionAuthenticated(connection_.get()))
        .WillOnce(InvokeWithoutArgs(
            this, &WebrtcConnectionToClientTest::ConnectStubs));
    EXPECT_CALL(handler_, OnConnectionChannelsConnected(connection_.get()));
    session_->event_handler()->OnSessionStateChange(Session::ACCEPTED);
    session_->event_handler()->OnSessionStateChange(Session::AUTHENTICATED);
    session_->event_handler()->OnSessionStateChange(Session::CONNECTED);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    connection_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ConnectStubs() {
    connection_->set_clipboard_stub(&clipboard_stub_);
    connection_->set_host_stub(&host_stub_);
    connection_->set_input_stub(&input_stub_);
  }

  base::MessageLoop message_loop_;
  MockConnectionToClientEventHandler handler_;
  MockClipboardStub clipboard_stub_;
  MockHostStub host_stub_;
  MockInputStub input_stub_;
  scoped_ptr<ConnectionToClient> connection_;

  FakeSession* session_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebrtcConnectionToClientTest);
};

TEST_F(WebrtcConnectionToClientTest, SendCapabilitesMessage) {
  Capabilities capabilities;
  connection_->client_stub()->SetCapabilities(capabilities);

  base::RunLoop().RunUntilIdle();

  // Verify that something has been written.
  // TODO(sergeyu): Verify that the correct data has been written.
  FakeStreamSocket* channel =
      session_->GetTransport()->GetStreamChannelFactory()->GetFakeChannel(
          kControlChannelName);
  ASSERT_TRUE(channel);
  EXPECT_FALSE(channel->written_data().empty());

  ControlMessage message;
  message.mutable_capabilities()->CopyFrom(capabilities);
  scoped_refptr<net::IOBufferWithSize> expected_message =
      SerializeAndFrameMessage(message);
  EXPECT_EQ(std::string(expected_message->data(), expected_message->size()),
            channel->written_data());

  // And then close the connection to ConnectionToClient.
  connection_->Disconnect(protocol::OK);

  base::RunLoop().RunUntilIdle();
}

// TODO(sergeyu): Add more tests here after Session is refactored not to own
// Transport, which will allow to use real WebrtcTransport with FakeSession.

}  // namespace protocol
}  // namespace remoting
