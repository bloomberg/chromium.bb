// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_test_server.h"

namespace net {

namespace tools {

namespace test {

class QuicTestDispatcher : public QuicDispatcher {
 public:
  QuicTestDispatcher(const QuicConfig& config,
                     const QuicCryptoServerConfig& crypto_config,
                     const QuicVersionVector& versions,
                     PacketWriterFactory* factory,
                     EpollServer* eps)
      : QuicDispatcher(config, crypto_config, versions, factory, eps) {}
  QuicSession* CreateQuicSession(QuicConnectionId id,
                                 const IPEndPoint& server,
                                 const IPEndPoint& client) override {
    if (session_creator_ == nullptr) {
      return QuicDispatcher::CreateQuicSession(id, server, client);
    } else {
      QuicConnection* connection = CreateQuicConnection(id, server, client);
      QuicServerSession* session =
          (session_creator_)(config(), connection, this);
      session->InitializeSession(crypto_config());
      return session;
    }
  }

  void set_session_creator(
      const QuicTestServer::SessionCreationFunction& function) {
    session_creator_ = function;
  }

 private:
  QuicTestServer::SessionCreationFunction session_creator_;
};

QuicDispatcher* QuicTestServer::CreateQuicDispatcher() {
  return new QuicTestDispatcher(
      config(), crypto_config(), supported_versions(),
      new QuicDispatcher::DefaultPacketWriterFactory(), epoll_server());
}

void QuicTestServer::SetSessionCreator(
    const SessionCreationFunction& function) {
  static_cast<QuicTestDispatcher*>(dispatcher())->set_session_creator(function);
}

///////////////////////////   TEST SESSIONS ///////////////////////////////

ImmediateGoAwaySession::ImmediateGoAwaySession(
    const QuicConfig& config,
    QuicConnection* connection,
    QuicServerSessionVisitor* visitor)
    : QuicServerSession(config, connection, visitor) {
  SendGoAway(QUIC_PEER_GOING_AWAY, "");
}

}  // namespace test

}  // namespace tools

}  // namespace net
