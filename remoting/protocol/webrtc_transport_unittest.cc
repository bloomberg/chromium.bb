// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_transport.h"

#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/message_channel_factory.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

const char kChannelName[] = "test_channel";

class TestTransportEventHandler : public WebrtcTransport::EventHandler {
 public:
  typedef base::Callback<void(ErrorCode error)> ErrorCallback;

  TestTransportEventHandler() {}
  ~TestTransportEventHandler() {}

  // All callbacks must be set before the test handler is passed to a Transport
  // object.
  void set_connecting_callback(const base::Closure& callback) {
    connecting_callback_ = callback;
  }
  void set_connected_callback(const base::Closure& callback) {
    connected_callback_ = callback;
  }
  void set_error_callback(const ErrorCallback& callback) {
    error_callback_ = callback;
  }

  // WebrtcTransport::EventHandler interface.
  void OnWebrtcTransportConnecting() override {
    if (!connecting_callback_.is_null())
      connecting_callback_.Run();
  }
  void OnWebrtcTransportConnected() override {
    if (!connected_callback_.is_null())
      connected_callback_.Run();
  }
  void OnWebrtcTransportError(ErrorCode error) override {
    error_callback_.Run(error);
  }
  void OnWebrtcTransportMediaStreamAdded(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
  void OnWebrtcTransportMediaStreamRemoved(
      scoped_refptr<webrtc::MediaStreamInterface> stream) override {}

 private:
  base::Closure connecting_callback_;
  base::Closure connected_callback_;
  ErrorCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestTransportEventHandler);
};

}  // namespace

class WebrtcTransportTest : public testing::Test {
 public:
  WebrtcTransportTest() {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  void TearDown() override {
    run_loop_.reset();
    client_message_pipe_.reset();
    client_transport_.reset();
    host_message_pipe_.reset();
    host_transport_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void ProcessTransportInfo(scoped_ptr<WebrtcTransport>* target_transport,
                            scoped_ptr<buzz::XmlElement> transport_info) {
    ASSERT_TRUE(target_transport);
    EXPECT_TRUE((*target_transport)
                    ->ProcessTransportInfo(transport_info.get()));
  }

  void InitializeConnection() {
    host_transport_.reset(
        new WebrtcTransport(jingle_glue::JingleThreadWrapper::current(),
                            TransportContext::ForTests(TransportRole::SERVER),
                            &host_event_handler_));
    host_authenticator_.reset(new FakeAuthenticator(
        FakeAuthenticator::HOST, 0, FakeAuthenticator::ACCEPT, false));

    client_transport_.reset(
        new WebrtcTransport(jingle_glue::JingleThreadWrapper::current(),
                            TransportContext::ForTests(TransportRole::CLIENT),
                            &client_event_handler_));
    client_authenticator_.reset(new FakeAuthenticator(
        FakeAuthenticator::CLIENT, 0, FakeAuthenticator::ACCEPT, false));
  }

  void StartConnection() {
    host_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));
    client_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));

    host_event_handler_.set_error_callback(base::Bind(
        &WebrtcTransportTest::OnSessionError, base::Unretained(this)));
    client_event_handler_.set_error_callback(base::Bind(
        &WebrtcTransportTest::OnSessionError, base::Unretained(this)));

    // Start both transports.
    host_transport_->Start(
        host_authenticator_.get(),
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &client_transport_));
    client_transport_->Start(
        client_authenticator_.get(),
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &host_transport_));
  }

  void WaitUntilConnected() {
    int counter = 2;
    host_event_handler_.set_connected_callback(
        base::Bind(&WebrtcTransportTest::QuitRunLoopOnCounter,
                   base::Unretained(this), &counter));
    client_event_handler_.set_connected_callback(
        base::Bind(&WebrtcTransportTest::QuitRunLoopOnCounter,
                   base::Unretained(this), &counter));

    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();

    host_event_handler_.set_connected_callback(base::Closure());
    client_event_handler_.set_connected_callback(base::Closure());

    EXPECT_EQ(OK, error_);
  }

  void CreateClientDataStream() {
    client_transport_->incoming_channel_factory()->CreateChannel(
        kChannelName, base::Bind(&WebrtcTransportTest::OnClientChannelCreated,
                                 base::Unretained(this)));
  }

  void CreateHostDataStream() {
    host_transport_->outgoing_channel_factory()->CreateChannel(
        kChannelName, base::Bind(&WebrtcTransportTest::OnHostChannelCreated,
                                 base::Unretained(this)));
  }

  void OnClientChannelCreated(scoped_ptr<MessagePipe> pipe) {
    client_message_pipe_ = std::move(pipe);
    if (run_loop_ && host_message_pipe_)
      run_loop_->Quit();
  }

  void OnHostChannelCreated(scoped_ptr<MessagePipe> pipe) {
    host_message_pipe_ = std::move(pipe);
    if (run_loop_ && client_message_pipe_)
      run_loop_->Quit();
  }

  void OnSessionError(ErrorCode error) {
    error_ = error;
    run_loop_->Quit();
  }

  void QuitRunLoopOnCounter(int* counter) {
    --(*counter);
    if (*counter == 0)
      run_loop_->Quit();
  }

 protected:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  NetworkSettings network_settings_;

  scoped_ptr<WebrtcTransport> host_transport_;
  TestTransportEventHandler host_event_handler_;
  scoped_ptr<FakeAuthenticator> host_authenticator_;

  scoped_ptr<WebrtcTransport> client_transport_;
  TestTransportEventHandler client_event_handler_;
  scoped_ptr<FakeAuthenticator> client_authenticator_;

  scoped_ptr<MessagePipe> client_message_pipe_;
  scoped_ptr<MessagePipe> host_message_pipe_;

  ErrorCode error_ = OK;
};

TEST_F(WebrtcTransportTest, Connects) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();
}

TEST_F(WebrtcTransportTest, DataStream) {
  client_event_handler_.set_connecting_callback(base::Bind(
      &WebrtcTransportTest::CreateClientDataStream, base::Unretained(this)));
  host_event_handler_.set_connecting_callback(base::Bind(
      &WebrtcTransportTest::CreateHostDataStream, base::Unretained(this)));

  InitializeConnection();
  StartConnection();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_TRUE(client_message_pipe_);
  EXPECT_TRUE(host_message_pipe_);

  const int kMessageSize = 1024;
  const int kMessages = 100;
  MessagePipeConnectionTester tester(host_message_pipe_.get(),
                                     client_message_pipe_.get(), kMessageSize,
                                     kMessages);
  tester.RunAndCheckResults();
}

// Verify that data streams can be created after connection has been initiated.
TEST_F(WebrtcTransportTest, DataStreamLate) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();

  CreateClientDataStream();
  CreateHostDataStream();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  EXPECT_TRUE(client_message_pipe_);
  EXPECT_TRUE(host_message_pipe_);
}

}  // namespace protocol
}  // namespace remoting
