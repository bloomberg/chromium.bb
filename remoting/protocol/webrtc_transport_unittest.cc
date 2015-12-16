// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_transport.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {
namespace protocol {

namespace {

const char kTestJid[] = "client@gmail.com/321";
const char kChannelName[] = "test_channel";

class TestTransportEventHandler : public Transport::EventHandler {
 public:
  typedef base::Callback<void(scoped_ptr<buzz::XmlElement> message)>
      TransportInfoCallback;
  typedef base::Callback<void(ErrorCode error)> ErrorCallback;

  TestTransportEventHandler() {}
  ~TestTransportEventHandler() {}

  // Both callback must be set before the test handler is passed to a Transport
  // object.
  void set_transport_info_callback(const TransportInfoCallback& callback) {
    transport_info_callback_ = callback;
  }
  void set_connected_callback(const base::Closure& callback) {
    connected_callback_ = callback;
  }
  void set_error_callback(const ErrorCallback& callback) {
    error_callback_ = callback;
  }

  // Transport::EventHandler interface.
  void OnOutgoingTransportInfo(scoped_ptr<buzz::XmlElement> message) override {
    transport_info_callback_.Run(message.Pass());
  }
  void OnTransportRouteChange(const std::string& channel_name,
                              const TransportRoute& route) override {}
  void OnTransportConnected() override {
    connected_callback_.Run();
  }
  void OnTransportError(ErrorCode error) override {
    error_callback_.Run(error);
  }

 private:
  TransportInfoCallback transport_info_callback_;
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

  void ProcessTransportInfo(scoped_ptr<Transport>* target_transport,
                            scoped_ptr<buzz::XmlElement> transport_info) {
    ASSERT_TRUE(target_transport);
    EXPECT_TRUE((*target_transport)
                    ->ProcessTransportInfo(transport_info.get()));
  }

 protected:
  void InitializeConnection() {
    signal_strategy_.reset(new FakeSignalStrategy(kTestJid));

    host_transport_factory_.reset(new WebrtcTransportFactory(
        jingle_glue::JingleThreadWrapper::current(),
        new TransportContext(
            signal_strategy_.get(),
            make_scoped_ptr(new ChromiumPortAllocatorFactory(nullptr)),
            network_settings_, TransportRole::SERVER)));
    host_transport_ = host_transport_factory_->CreateTransport();
    host_authenticator_.reset(new FakeAuthenticator(
        FakeAuthenticator::HOST, 0, FakeAuthenticator::ACCEPT, false));

    client_transport_factory_.reset(new WebrtcTransportFactory(
        jingle_glue::JingleThreadWrapper::current(),
        new TransportContext(
            signal_strategy_.get(),
            make_scoped_ptr(new ChromiumPortAllocatorFactory(nullptr)),
            network_settings_, TransportRole::CLIENT)));
    client_transport_ = client_transport_factory_->CreateTransport();
    host_authenticator_.reset(new FakeAuthenticator(
        FakeAuthenticator::CLIENT, 0, FakeAuthenticator::ACCEPT, false));

    // Connect signaling between the two WebrtcTransport objects.
    host_event_handler_.set_transport_info_callback(
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &client_transport_));
    client_event_handler_.set_transport_info_callback(
        base::Bind(&WebrtcTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &host_transport_));
  }

  void StartConnection() {
    host_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));
    client_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));

    host_event_handler_.set_error_callback(base::Bind(
        &WebrtcTransportTest::OnSessionError, base::Unretained(this)));
    client_event_handler_.set_error_callback(base::Bind(
        &WebrtcTransportTest::OnSessionError, base::Unretained(this)));

    host_transport_->Start(&host_event_handler_, host_authenticator_.get());
    client_transport_->Start(&client_event_handler_,
                             client_authenticator_.get());
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

    EXPECT_EQ(OK, error_);
  }

  void CreateDataStream() {
    client_transport_->GetStreamChannelFactory()->CreateChannel(
        kChannelName, base::Bind(&WebrtcTransportTest::OnClientChannelCreated,
                                 base::Unretained(this)));
    host_transport_->GetStreamChannelFactory()->CreateChannel(
        kChannelName, base::Bind(&WebrtcTransportTest::OnHostChannelCreated,
                                 base::Unretained(this)));
  }

  void OnClientChannelCreated(scoped_ptr<P2PStreamSocket> socket) {
    client_socket_ = socket.Pass();
    if (run_loop_ && host_socket_)
      run_loop_->Quit();
  }

  void OnHostChannelCreated(scoped_ptr<P2PStreamSocket> socket) {
    host_socket_ = socket.Pass();
    if (run_loop_ && client_socket_)
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

  scoped_ptr< FakeSignalStrategy> signal_strategy_;

  scoped_ptr<WebrtcTransportFactory> host_transport_factory_;
  scoped_ptr<Transport> host_transport_;
  TestTransportEventHandler host_event_handler_;
  scoped_ptr<FakeAuthenticator> host_authenticator_;

  scoped_ptr<WebrtcTransportFactory> client_transport_factory_;
  scoped_ptr<Transport> client_transport_;
  TestTransportEventHandler client_event_handler_;
  scoped_ptr<FakeAuthenticator> client_authenticator_;

  scoped_ptr<P2PStreamSocket> client_socket_;
  scoped_ptr<P2PStreamSocket> host_socket_;

  ErrorCode error_ = OK;
};

TEST_F(WebrtcTransportTest, Connects) {
  InitializeConnection();
  StartConnection();
  WaitUntilConnected();
}

TEST_F(WebrtcTransportTest, DataStream) {
  InitializeConnection();
  CreateDataStream();
  StartConnection();

  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();

  const int kMessageSize = 1024;
  const int kMessages = 100;
  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

}  // namespace protocol
}  // namespace remoting
