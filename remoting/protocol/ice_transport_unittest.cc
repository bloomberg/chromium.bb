// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/stream_channel_factory.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

using testing::_;

namespace remoting {
namespace protocol {

namespace {

// Send 100 messages 1024 bytes each. UDP messages are sent with 10ms delay
// between messages (about 1 second for 100 messages).
const int kMessageSize = 1024;
const int kMessages = 100;
const char kChannelName[] = "test_channel";

ACTION_P(QuitRunLoop, run_loop) {
  run_loop->Quit();
}

ACTION_P2(QuitRunLoopOnCounter, run_loop, counter) {
  --(*counter);
  EXPECT_GE(*counter, 0);
  if (*counter == 0)
    run_loop->Quit();
}

class MockChannelCreatedCallback {
 public:
  MOCK_METHOD1(OnDone, void(P2PStreamSocket* socket));
};

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
    transport_info_callback_.Run(std::move(message));
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

class IceTransportTest : public testing::Test {
 public:
  IceTransportTest() {
    jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  void TearDown() override {
    client_socket_.reset();
    host_socket_.reset();
    client_transport_.reset();
    host_transport_.reset();
    message_loop_.RunUntilIdle();
  }

  void ProcessTransportInfo(scoped_ptr<IceTransport>* target_transport,
                            scoped_ptr<buzz::XmlElement> transport_info) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&IceTransportTest::DeliverTransportInfo,
                              base::Unretained(this), target_transport,
                              base::Passed(&transport_info)),
        transport_info_delay_);
  }

  void DeliverTransportInfo(scoped_ptr<IceTransport>* target_transport,
                            scoped_ptr<buzz::XmlElement> transport_info) {
    ASSERT_TRUE(target_transport);
    EXPECT_TRUE(
        (*target_transport)->ProcessTransportInfo(transport_info.get()));
  }

  void InitializeConnection() {
    host_transport_.reset(new IceTransport(new TransportContext(
        signal_strategy_.get(),
        make_scoped_ptr(new ChromiumPortAllocatorFactory(nullptr)),
        network_settings_, TransportRole::SERVER)));
    if (!host_authenticator_) {
      host_authenticator_.reset(new FakeAuthenticator(
          FakeAuthenticator::HOST, 0, FakeAuthenticator::ACCEPT, true));
    }

    client_transport_.reset(new IceTransport(new TransportContext(
        signal_strategy_.get(),
        make_scoped_ptr(new ChromiumPortAllocatorFactory(nullptr)),
        network_settings_, TransportRole::CLIENT)));
    if (!client_authenticator_) {
      client_authenticator_.reset(new FakeAuthenticator(
          FakeAuthenticator::CLIENT, 0, FakeAuthenticator::ACCEPT, true));
    }

    // Connect signaling between the two IceTransport objects.
    host_event_handler_.set_transport_info_callback(
        base::Bind(&IceTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &client_transport_));
    client_event_handler_.set_transport_info_callback(
        base::Bind(&IceTransportTest::ProcessTransportInfo,
                   base::Unretained(this), &host_transport_));

    host_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));
    host_event_handler_.set_error_callback(base::Bind(
        &IceTransportTest::OnTransportError, base::Unretained(this)));

    client_event_handler_.set_connected_callback(base::Bind(&base::DoNothing));
    client_event_handler_.set_error_callback(base::Bind(
        &IceTransportTest::OnTransportError, base::Unretained(this)));

    host_transport_->Start(&host_event_handler_, host_authenticator_.get());
    client_transport_->Start(&client_event_handler_,
                             client_authenticator_.get());
  }

  void WaitUntilConnected() {
    run_loop_.reset(new base::RunLoop());

    int counter = 2;
    EXPECT_CALL(client_channel_callback_, OnDone(_))
        .WillOnce(QuitRunLoopOnCounter(run_loop_.get(), &counter));
    EXPECT_CALL(host_channel_callback_, OnDone(_))
        .WillOnce(QuitRunLoopOnCounter(run_loop_.get(), &counter));

    run_loop_->Run();

    EXPECT_TRUE(client_socket_.get());
    EXPECT_TRUE(host_socket_.get());
  }

  void OnClientChannelCreated(scoped_ptr<P2PStreamSocket> socket) {
    client_socket_ = std::move(socket);
    client_channel_callback_.OnDone(client_socket_.get());
  }

  void OnHostChannelCreated(scoped_ptr<P2PStreamSocket> socket) {
    host_socket_ = std::move(socket);
    host_channel_callback_.OnDone(host_socket_.get());
  }

  void OnTransportError(ErrorCode error) {
    error_ = error;
    run_loop_->Quit();
  }

 protected:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  NetworkSettings network_settings_;

  scoped_ptr<FakeSignalStrategy> signal_strategy_;

  base::TimeDelta transport_info_delay_;

  scoped_ptr<IceTransport> host_transport_;
  TestTransportEventHandler host_event_handler_;
  scoped_ptr<FakeAuthenticator> host_authenticator_;

  scoped_ptr<IceTransport> client_transport_;
  TestTransportEventHandler client_event_handler_;
  scoped_ptr<FakeAuthenticator> client_authenticator_;

  MockChannelCreatedCallback client_channel_callback_;
  MockChannelCreatedCallback host_channel_callback_;

  scoped_ptr<P2PStreamSocket> client_socket_;
  scoped_ptr<P2PStreamSocket> host_socket_;

  ErrorCode error_ = OK;
};

TEST_F(IceTransportTest, DataStream) {
  InitializeConnection();

  client_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnHostChannelCreated,
                               base::Unretained(this)));

  WaitUntilConnected();

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

TEST_F(IceTransportTest, MuxDataStream) {
  InitializeConnection();

  client_transport_->GetMultiplexedChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_transport_->GetMultiplexedChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnHostChannelCreated,
                               base::Unretained(this)));

  WaitUntilConnected();

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}

TEST_F(IceTransportTest, FailedChannelAuth) {
  // Use host authenticator with one that rejects channel authentication.
  host_authenticator_.reset(new FakeAuthenticator(
      FakeAuthenticator::HOST, 0, FakeAuthenticator::REJECT_CHANNEL, true));

  InitializeConnection();

  client_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnHostChannelCreated,
                               base::Unretained(this)));

  run_loop_.reset(new base::RunLoop());

  EXPECT_CALL(host_channel_callback_, OnDone(nullptr))
      .WillOnce(QuitRunLoop(run_loop_.get()));

  run_loop_->Run();

  EXPECT_FALSE(host_socket_);

  client_transport_->GetStreamChannelFactory()->CancelChannelCreation(
      kChannelName);
}

// Verify that channels are never marked connected if connection cannot be
// established.
TEST_F(IceTransportTest, TestBrokenTransport) {
  // Allow only incoming connections on both ends, which effectively renders
  // transport unusable.
  network_settings_ = NetworkSettings(NetworkSettings::NAT_TRAVERSAL_DISABLED);

  InitializeConnection();

  client_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnHostChannelCreated,
                               base::Unretained(this)));

  message_loop_.RunUntilIdle();

  // Verify that neither of the two ends of the channel is connected.
  EXPECT_FALSE(client_socket_);
  EXPECT_FALSE(host_socket_);

  client_transport_->GetStreamChannelFactory()->CancelChannelCreation(
      kChannelName);
  host_transport_->GetStreamChannelFactory()->CancelChannelCreation(
      kChannelName);
}

TEST_F(IceTransportTest, TestCancelChannelCreation) {
  InitializeConnection();

  client_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  client_transport_->GetStreamChannelFactory()->CancelChannelCreation(
      kChannelName);

  EXPECT_TRUE(!client_socket_.get());
}

// Verify that we can still connect even when there is a delay in signaling
// messages delivery.
TEST_F(IceTransportTest, TestDelayedSignaling) {
  transport_info_delay_ = base::TimeDelta::FromMilliseconds(100);

  InitializeConnection();

  client_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnClientChannelCreated,
                               base::Unretained(this)));
  host_transport_->GetStreamChannelFactory()->CreateChannel(
      kChannelName, base::Bind(&IceTransportTest::OnHostChannelCreated,
                               base::Unretained(this)));

  WaitUntilConnected();

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);
  tester.Start();
  message_loop_.Run();
  tester.CheckResults();
}


}  // namespace protocol
}  // namespace remoting
