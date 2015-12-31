// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/ice_connection_to_host.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_connection_to_client.h"
#include "remoting/protocol/webrtc_connection_to_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace remoting {
namespace protocol {

namespace {

MATCHER_P(EqualsCapabilitiesMessage, message, "") {
  return arg.capabilities() == message.capabilities();
}

MATCHER_P(EqualsKeyEvent, event, "") {
  return arg.usb_keycode() == event.usb_keycode() &&
         arg.pressed() == event.pressed();
}

class MockConnectionToHostEventCallback
    : public ConnectionToHost::HostEventCallback {
 public:
  MockConnectionToHostEventCallback() {}
  ~MockConnectionToHostEventCallback() override {}

  MOCK_METHOD2(OnConnectionState,
               void(ConnectionToHost::State state, ErrorCode error));
  MOCK_METHOD1(OnConnectionReady, void(bool ready));
  MOCK_METHOD2(OnRouteChanged,
               void(const std::string& channel_name,
                    const TransportRoute& route));
};

}  // namespace

class ConnectionTest : public testing::Test,
                       public testing::WithParamInterface<bool> {
 public:
  ConnectionTest() {}

 protected:
  void SetUp() override {
    // Create fake sessions.
    host_session_ = new FakeSession();
    owned_client_session_.reset(new FakeSession());
    client_session_ = owned_client_session_.get();

    // Create Connection objects
    if (GetParam()) {
      host_connection_.reset(new WebrtcConnectionToClient(
          make_scoped_ptr(host_session_),
          TransportContext::ForTests(protocol::TransportRole::SERVER)));
      client_connection_.reset(new WebrtcConnectionToHost());

    } else {
      host_connection_.reset(new IceConnectionToClient(
          make_scoped_ptr(host_session_),
          TransportContext::ForTests(protocol::TransportRole::SERVER),
          message_loop_.task_runner()));
      client_connection_.reset(new IceConnectionToHost());
    }

    // Setup host side.
    host_connection_->SetEventHandler(&host_event_handler_);
    host_connection_->set_clipboard_stub(&host_clipboard_stub_);
    host_connection_->set_host_stub(&host_stub_);
    host_connection_->set_input_stub(&host_input_stub_);

    // Setup client side.
    client_connection_->set_client_stub(&client_stub_);
    client_connection_->set_clipboard_stub(&client_clipboard_stub_);
    client_connection_->set_video_stub(&client_video_stub_);
  }

  void Connect() {
    {
      testing::InSequence sequence;
      EXPECT_CALL(host_event_handler_,
                  OnConnectionAuthenticating(host_connection_.get()));
      EXPECT_CALL(host_event_handler_,
                  OnConnectionAuthenticated(host_connection_.get()));
    }
    EXPECT_CALL(host_event_handler_,
                OnConnectionChannelsConnected(host_connection_.get()))
        .WillOnce(
            InvokeWithoutArgs(this, &ConnectionTest::OnHostConnected));
    EXPECT_CALL(host_event_handler_, OnRouteChange(_, _, _))
        .Times(testing::AnyNumber());

    {
      testing::InSequence sequence;
      EXPECT_CALL(client_event_handler_,
                  OnConnectionState(ConnectionToHost::CONNECTING, OK));
      EXPECT_CALL(client_event_handler_,
                  OnConnectionState(ConnectionToHost::AUTHENTICATED, OK));
      EXPECT_CALL(client_event_handler_,
                  OnConnectionState(ConnectionToHost::CONNECTED, OK))
          .WillOnce(InvokeWithoutArgs(
              this, &ConnectionTest::OnClientConnected));
    }
    EXPECT_CALL(client_event_handler_, OnRouteChanged(_, _))
        .Times(testing::AnyNumber());

    client_connection_->Connect(
        std::move(owned_client_session_),
        TransportContext::ForTests(protocol::TransportRole::CLIENT),
        &client_event_handler_);
    client_session_->SimulateConnection(host_session_);

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();

    EXPECT_TRUE(client_connected_);
    EXPECT_TRUE(host_connected_);
  }

  void TearDown() override {
    client_connection_.reset();
    host_connection_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnHostConnected() {
    host_connected_ = true;
    if (client_connected_ && run_loop_)
      run_loop_->Quit();
  }

  void OnClientConnected() {
    client_connected_ = true;
    if (host_connected_ && run_loop_)
      run_loop_->Quit();
  }

  base::MessageLoopForIO message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  MockConnectionToClientEventHandler host_event_handler_;
  MockClipboardStub host_clipboard_stub_;
  MockHostStub host_stub_;
  MockInputStub host_input_stub_;
  scoped_ptr<ConnectionToClient> host_connection_;
  FakeSession* host_session_;  // Owned by |host_connection_|.
  bool host_connected_ = false;

  MockConnectionToHostEventCallback client_event_handler_;
  MockClientStub client_stub_;
  MockClipboardStub client_clipboard_stub_;
  MockVideoStub client_video_stub_;
  scoped_ptr<ConnectionToHost> client_connection_;
  FakeSession* client_session_;  // Owned by |client_connection_|.
  scoped_ptr<FakeSession> owned_client_session_;
  bool client_connected_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionTest);
};

// TODO(sergeyu): Disabled due to failures on Vista bots. See
// https://crbug.com/573366.
// INSTANTIATE_TEST_CASE_P(Ice, ConnectionTest, ::testing::Values(false));
INSTANTIATE_TEST_CASE_P(Webrtc, ConnectionTest, ::testing::Values(true));

TEST_P(ConnectionTest, RejectConnection) {
  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CONNECTING, OK));
  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CLOSED, OK));

  client_connection_->Connect(
      std::move(owned_client_session_),
      TransportContext::ForTests(protocol::TransportRole::CLIENT),
      &client_event_handler_);
  client_session_->event_handler()->OnSessionStateChange(Session::CLOSED);
}

TEST_P(ConnectionTest, Disconnect) {
  Connect();

  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CLOSED, OK));
  EXPECT_CALL(host_event_handler_,
              OnConnectionClosed(host_connection_.get(), OK));

  client_session_->Close(OK);
  base::RunLoop().RunUntilIdle();
}

TEST_P(ConnectionTest, Control) {
  Connect();

  Capabilities capabilities_msg;
  capabilities_msg.set_capabilities("test_capability");

  EXPECT_CALL(client_stub_,
              SetCapabilities(EqualsCapabilitiesMessage(capabilities_msg)));

  // Send capabilities from the host.
  host_connection_->client_stub()->SetCapabilities(capabilities_msg);

  base::RunLoop().RunUntilIdle();
}

TEST_P(ConnectionTest, Events) {
  Connect();

  KeyEvent event;
  event.set_usb_keycode(3);
  event.set_pressed(true);

  EXPECT_CALL(host_event_handler_,
              OnInputEventReceived(host_connection_.get(), _));
  EXPECT_CALL(host_input_stub_, InjectKeyEvent(EqualsKeyEvent(event)));

  // Send capabilities from the client.
  client_connection_->input_stub()->InjectKeyEvent(event);

  base::RunLoop().RunUntilIdle();
}

}  // namespace protocol
}  // namespace remoting
