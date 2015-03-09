// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/xmpp_signal_strategy.h"

#include "base/base64.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/libjingle/xmllite/xmlelement.h"

namespace remoting {

namespace {

class XmppSocketDataProvider: public net::SocketDataProvider {
 public:
  net::MockRead GetNextRead() override {
    return net::MockRead(net::ASYNC, net::ERR_IO_PENDING);
  }

  net::MockWriteResult OnWrite(const std::string& data) override {
    written_data_.append(data);
    return net::MockWriteResult(net::SYNCHRONOUS, data.size());
  }

  void Reset() override {}

  void ReceiveData(const std::string& text) {
    socket()->OnReadComplete(
        net::MockRead(net::ASYNC, text.data(), text.size()));
  }

  void Close() {
    ReceiveData(std::string());
  }

  void SimulateNetworkError() {
    socket()->OnReadComplete(
        net::MockRead(net::ASYNC, net::ERR_CONNECTION_RESET));
  }

  std::string GetAndClearWrittenData() {
    std::string data;
    data.swap(written_data_);
    return data;
  }

 private:
  std::string written_data_;
};

}  // namespace

const char kTestUsername[] = "test_username@example.com";
const char kTestAuthToken[] = "test_auth_token";

class XmppSignalStrategyTest : public testing::Test,
                               public SignalStrategy::Listener {
 public:
  XmppSignalStrategyTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

  void SetUp() override {
    scoped_ptr<net::TestURLRequestContext> context(
        new net::TestURLRequestContext());
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.task_runner(), context.Pass());

    XmppSignalStrategy::XmppServerConfig config;
    config.host = "talk.google.com";
    config.port = 443;
    config.username = kTestUsername;
    config.auth_token = kTestAuthToken;
    signal_strategy_.reset(new XmppSignalStrategy(
        &client_socket_factory_, request_context_getter_, config));
    signal_strategy_->AddListener(this);
  }

  void TearDown() override {
    signal_strategy_->RemoveListener(this);
    signal_strategy_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnSignalStrategyStateChange(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }

  bool OnSignalStrategyIncomingStanza(const buzz::XmlElement* stanza) override {
    received_messages_.push_back(
        make_scoped_ptr(new buzz::XmlElement(*stanza)));
    return true;
  }

  void Connect(bool success);

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::MockClientSocketFactory client_socket_factory_;
  scoped_ptr<XmppSocketDataProvider> socket_data_provider_;
  scoped_ptr<net::SSLSocketDataProvider> ssl_socket_data_provider_;
  scoped_ptr<XmppSignalStrategy> signal_strategy_;

  std::vector<SignalStrategy::State> state_history_;
  ScopedVector<buzz::XmlElement> received_messages_;
};

void XmppSignalStrategyTest::Connect(bool success) {
  EXPECT_EQ(SignalStrategy::DISCONNECTED, signal_strategy_->GetState());
  state_history_.clear();

  socket_data_provider_.reset(new XmppSocketDataProvider());
  socket_data_provider_->set_connect_data(
      net::MockConnect(net::ASYNC, net::OK));
  client_socket_factory_.AddSocketDataProvider(socket_data_provider_.get());

  ssl_socket_data_provider_.reset(
      new net::SSLSocketDataProvider(net::ASYNC, net::OK));
  client_socket_factory_.AddSSLSocketDataProvider(
      ssl_socket_data_provider_.get());

  signal_strategy_->Connect();

  EXPECT_EQ(SignalStrategy::CONNECTING, signal_strategy_->GetState());
  EXPECT_EQ(1U, state_history_.size());
  EXPECT_EQ(SignalStrategy::CONNECTING, state_history_[0]);

  // No data written before TLS.
  EXPECT_EQ("", socket_data_provider_->GetAndClearWrittenData());

  base::RunLoop().RunUntilIdle();

  socket_data_provider_->ReceiveData(
      "<stream:stream from=\"google.com\" id=\"DCDDE5171CB2154A\" "
        "version=\"1.0\" "
        "xmlns:stream=\"http://etherx.jabber.org/streams\" "
        "xmlns=\"jabber:client\">"
      "<stream:features>"
        "<mechanisms xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
          "<mechanism>X-OAUTH2</mechanism>"
          "<mechanism>X-GOOGLE-TOKEN</mechanism>"
          "<mechanism>PLAIN</mechanism>"
        "</mechanisms>"
      "</stream:features>");

  base::RunLoop().RunUntilIdle();

  std::string cookie;
  base::Base64Encode(std::string("\0", 1) + kTestUsername +
                         std::string("\0", 1) + kTestAuthToken,
                     &cookie);
  // Expect auth message.
  EXPECT_EQ(
      "<stream:stream to=\"google.com\" version=\"1.0\" "
          "xmlns=\"jabber:client\" "
          "xmlns:stream=\"http://etherx.jabber.org/streams\">"
      "<auth xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\" mechanism=\"X-OAUTH2\" "
          "auth:service=\"oauth2\" auth:allow-generated-jid=\"true\" "
          "auth:client-uses-full-bind-result=\"true\" "
          "auth:allow-non-google-login=\"true\" "
          "xmlns:auth=\"http://www.google.com/talk/protocol/auth\">" + cookie +
      "</auth>", socket_data_provider_->GetAndClearWrittenData());

  if (!success) {
    socket_data_provider_->ReceiveData(
        "<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\">"
        "<not-authorized/></failure>");
    EXPECT_EQ(2U, state_history_.size());
    EXPECT_EQ(SignalStrategy::DISCONNECTED, state_history_[1]);
    EXPECT_EQ(SignalStrategy::AUTHENTICATION_FAILED,
              signal_strategy_->GetError());
    return;
  }

  socket_data_provider_->ReceiveData(
      "<success xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>");

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      "<stream:stream to=\"google.com\" version=\"1.0\" "
        "xmlns=\"jabber:client\" "
        "xmlns:stream=\"http://etherx.jabber.org/streams\">"
      "<iq type=\"set\" id=\"0\">"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
          "<resource>chromoting</resource>"
        "</bind>"
      "</iq>"
      "<iq type=\"set\" id=\"1\">"
        "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
      "</iq>",
      socket_data_provider_->GetAndClearWrittenData());
  socket_data_provider_->ReceiveData(
      "<stream:stream from=\"google.com\" id=\"104FA10576E2AA80\" "
        "version=\"1.0\" "
        "xmlns:stream=\"http://etherx.jabber.org/streams\" "
        "xmlns=\"jabber:client\">"
      "<stream:features>"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>"
        "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
      "</stream:features>"
      "<iq id=\"0\" type=\"result\">"
        "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
          "<jid>" + std::string(kTestUsername) + "/chromoting52B4920E</jid>"
        "</bind>"
      "</iq>"
      "<iq type=\"result\" id=\"1\"/>");

  EXPECT_EQ(2U, state_history_.size());
  EXPECT_EQ(SignalStrategy::CONNECTED, state_history_[1]);
}

TEST_F(XmppSignalStrategyTest, SendAndReceive) {
  Connect(true);

  EXPECT_TRUE(signal_strategy_->SendStanza(make_scoped_ptr(
      new buzz::XmlElement(buzz::QName(std::string(), "hello")))));
  EXPECT_EQ("<hello/>", socket_data_provider_->GetAndClearWrittenData());

  socket_data_provider_->ReceiveData("<hi xmlns=\"hello\"/>");
  EXPECT_EQ(1U, received_messages_.size());
  EXPECT_EQ("<hi xmlns=\"hello\"/>", received_messages_[0]->Str());
}

TEST_F(XmppSignalStrategyTest, AuthError) {
  Connect(false);
}

TEST_F(XmppSignalStrategyTest, ConnectionClosed) {
  Connect(true);

  socket_data_provider_->Close();

  EXPECT_EQ(3U, state_history_.size());
  EXPECT_EQ(SignalStrategy::DISCONNECTED, state_history_[2]);
  EXPECT_EQ(SignalStrategy::DISCONNECTED, signal_strategy_->GetState());
  EXPECT_EQ(SignalStrategy::OK, signal_strategy_->GetError());

  // Can't send messages anymore.
  EXPECT_FALSE(signal_strategy_->SendStanza(make_scoped_ptr(
      new buzz::XmlElement(buzz::QName(std::string(), "hello")))));

  // Try connecting again.
  Connect(true);
}

TEST_F(XmppSignalStrategyTest, NetworkError) {
  Connect(true);

  socket_data_provider_->SimulateNetworkError();

  EXPECT_EQ(3U, state_history_.size());
  EXPECT_EQ(SignalStrategy::DISCONNECTED, state_history_[2]);
  EXPECT_EQ(SignalStrategy::NETWORK_ERROR, signal_strategy_->GetError());

  // Can't send messages anymore.
  EXPECT_FALSE(signal_strategy_->SendStanza(make_scoped_ptr(
      new buzz::XmlElement(buzz::QName(std::string(), "hello")))));

  // Try connecting again.
  Connect(true);
}

}  // namespace remoting
