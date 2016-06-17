// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/fake_video_renderer.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/ice_connection_to_host.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_connection_to_client.h"
#include "remoting/protocol/webrtc_connection_to_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

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

ACTION_P(QuitRunLoop, run_loop) {
  run_loop->Quit();
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

class TestScreenCapturer : public webrtc::DesktopCapturer {
 public:
  TestScreenCapturer() {}
  ~TestScreenCapturer() override {}

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override {
    callback_ = callback;
  }
  void Capture(const webrtc::DesktopRegion& region) override {
    // Return black 100x100 frame.
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(100, 100)));
    memset(frame->data(), 0, frame->stride() * frame->size().height());

    // Set updated_region only for the first frame, as the frame content
    // doesn't change.
    if (!first_frame_sent_) {
      first_frame_sent_ = true;
      frame->mutable_updated_region()->SetRect(
          webrtc::DesktopRect::MakeSize(frame->size()));
    }

    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

 private:
  Callback* callback_ = nullptr;
  bool first_frame_sent_ = false;
};

}  // namespace

class ConnectionTest : public testing::Test,
                       public testing::WithParamInterface<bool> {
 public:
  ConnectionTest() {}

 protected:
  bool is_using_webrtc() { return GetParam(); }

  void SetUp() override {
    // Create fake sessions.
    host_session_ = new FakeSession();
    owned_client_session_.reset(new FakeSession());
    client_session_ = owned_client_session_.get();

    // Create Connection objects.
    if (is_using_webrtc()) {
      host_connection_.reset(new WebrtcConnectionToClient(
          base::WrapUnique(host_session_),
          TransportContext::ForTests(protocol::TransportRole::SERVER),
          message_loop_.task_runner()));
      client_connection_.reset(new WebrtcConnectionToHost());

    } else {
      host_connection_.reset(new IceConnectionToClient(
          base::WrapUnique(host_session_),
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
    client_connection_->set_video_renderer(&client_video_renderer_);
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

  void WaitFirstVideoFrame() {
    base::RunLoop run_loop;

    // Expect frames to be passed to FrameConsumer when WebRTC is used, or to
    // VideoStub otherwise.
    if (is_using_webrtc()) {
      client_video_renderer_.GetFrameConsumer()->set_on_frame_callback(
          base::Bind(&base::RunLoop::Quit, base::Unretained(&run_loop)));
    } else {
      client_video_renderer_.GetVideoStub()->set_on_frame_callback(
          base::Bind(&base::RunLoop::Quit, base::Unretained(&run_loop)));
    }

    run_loop.Run();

    if (is_using_webrtc()) {
      EXPECT_EQ(
          client_video_renderer_.GetFrameConsumer()->received_frames().size(),
          1U);
      EXPECT_EQ(
          client_video_renderer_.GetVideoStub()->received_packets().size(), 0U);
    } else {
      EXPECT_EQ(
          client_video_renderer_.GetFrameConsumer()->received_frames().size(),
          0U);
      EXPECT_EQ(
          client_video_renderer_.GetVideoStub()->received_packets().size(), 1U);
    }
  }

  base::MessageLoopForIO message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;

  MockConnectionToClientEventHandler host_event_handler_;
  MockClipboardStub host_clipboard_stub_;
  MockHostStub host_stub_;
  MockInputStub host_input_stub_;
  std::unique_ptr<ConnectionToClient> host_connection_;
  FakeSession* host_session_;  // Owned by |host_connection_|.
  bool host_connected_ = false;

  MockConnectionToHostEventCallback client_event_handler_;
  MockClientStub client_stub_;
  MockClipboardStub client_clipboard_stub_;
  FakeVideoRenderer client_video_renderer_;
  std::unique_ptr<ConnectionToHost> client_connection_;
  FakeSession* client_session_;  // Owned by |client_connection_|.
  std::unique_ptr<FakeSession> owned_client_session_;
  bool client_connected_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionTest);
};

INSTANTIATE_TEST_CASE_P(Ice, ConnectionTest, ::testing::Values(false));
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

  base::RunLoop run_loop;

  EXPECT_CALL(client_stub_,
              SetCapabilities(EqualsCapabilitiesMessage(capabilities_msg)))
      .WillOnce(QuitRunLoop(&run_loop));

  // Send capabilities from the host.
  host_connection_->client_stub()->SetCapabilities(capabilities_msg);

  run_loop.Run();
}

TEST_P(ConnectionTest, Events) {
  Connect();

  KeyEvent event;
  event.set_usb_keycode(3);
  event.set_pressed(true);

  base::RunLoop run_loop;

  EXPECT_CALL(host_event_handler_,
              OnInputEventReceived(host_connection_.get(), _));
  EXPECT_CALL(host_input_stub_, InjectKeyEvent(EqualsKeyEvent(event)))
      .WillOnce(QuitRunLoop(&run_loop));

  // Send capabilities from the client.
  client_connection_->input_stub()->InjectKeyEvent(event);

  run_loop.Run();
}

TEST_P(ConnectionTest, Video) {
  Connect();

  std::unique_ptr<VideoStream> video_stream =
      host_connection_->StartVideoStream(
          base::WrapUnique(new TestScreenCapturer()));

  WaitFirstVideoFrame();
}

// Verifies that the VideoStream doesn't loose any video frames while the
// connection is being established.
TEST_P(ConnectionTest, VideoWithSlowSignaling) {
  // Add signaling delay to slow down connection handshake.
  host_session_->set_signaling_delay(base::TimeDelta::FromMilliseconds(100));
  client_session_->set_signaling_delay(base::TimeDelta::FromMilliseconds(100));

  Connect();

  std::unique_ptr<VideoStream> video_stream =
      host_connection_->StartVideoStream(
          base::WrapUnique(new TestScreenCapturer()));

  WaitFirstVideoFrame();
}

}  // namespace protocol
}  // namespace remoting
