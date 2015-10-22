// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_test_server.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_config.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_connection_helper.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"
#include "net/tools/quic/quic_server_session.h"
#include "net/tools/quic/quic_spdy_server_stream.h"

namespace net {
namespace tools {
namespace test {

class CustomStreamSession : public QuicServerSession {
 public:
  CustomStreamSession(const QuicConfig& config,
                      QuicConnection* connection,
                      QuicServerSessionVisitor* visitor,
                      const QuicCryptoServerConfig* crypto_config,
                      QuicTestServer::StreamCreationFunction creator)
      : QuicServerSession(config, connection, visitor, crypto_config),
        // TODO(rtenneti): use std::move when chromium supports it.
        stream_creator_(creator) {}

  QuicSpdyStream* CreateIncomingDynamicStream(QuicStreamId id) override {
    if (!ShouldCreateIncomingDynamicStream(id)) {
      return nullptr;
    }
    return stream_creator_(id, this);
  }

 private:
  QuicTestServer::StreamCreationFunction stream_creator_;
};

class QuicTestDispatcher : public QuicDispatcher {
 public:
  QuicTestDispatcher(const QuicConfig& config,
                     const QuicCryptoServerConfig* crypto_config,
                     const QuicVersionVector& versions,
                     PacketWriterFactory* factory,
                     QuicConnectionHelperInterface* helper)
      : QuicDispatcher(config, crypto_config, versions, factory, helper) {}
  QuicServerSession* CreateQuicSession(QuicConnectionId id,
                                       const IPEndPoint& client) override {
    if (session_creator_ == nullptr && stream_creator_ == nullptr) {
      return QuicDispatcher::CreateQuicSession(id, client);
    }
    QuicConnection* connection =
        new QuicConnection(id, client, helper(), connection_writer_factory(),
                           /* owns_writer= */ true, Perspective::IS_SERVER,
                           /* is_secure */ true, supported_versions());

    QuicServerSession* session = nullptr;
    if (stream_creator_ != nullptr) {
      session = new CustomStreamSession(config(), connection, this,
                                        crypto_config(), stream_creator_);
    } else {
      session = session_creator_(config(), connection, this, crypto_config());
    }
    session->Initialize();
    return session;
  }

  void set_session_creator(QuicTestServer::SessionCreationFunction function) {
    DCHECK(session_creator_ == nullptr);
    DCHECK(stream_creator_ == nullptr);
    // TODO(rtenneti): use std::move when chromium supports it.
    // session_creator_ = std::move(function);
    session_creator_ = function;
  }

  void set_stream_creator(QuicTestServer::StreamCreationFunction function) {
    DCHECK(session_creator_ == nullptr);
    DCHECK(stream_creator_ == nullptr);
    // TODO(rtenneti): use std::move when chromium supports it.
    // stream_creator_ = std::move(function);
    stream_creator_ = function;
  }

  QuicTestServer::SessionCreationFunction session_creator() {
    return session_creator_;
  }

  QuicTestServer::StreamCreationFunction stream_creator() {
    return stream_creator_;
  }

 private:
  QuicTestServer::SessionCreationFunction session_creator_;
  QuicTestServer::StreamCreationFunction stream_creator_;
};

QuicTestServer::QuicTestServer() : QuicServer() {}

QuicTestServer::QuicTestServer(const QuicConfig& config,
                               const QuicVersionVector& supported_versions)
    : QuicServer(config, supported_versions) {}

QuicDispatcher* QuicTestServer::CreateQuicDispatcher() {
  return new QuicTestDispatcher(
      config(), &crypto_config(), supported_versions(),
      new QuicDispatcher::DefaultPacketWriterFactory(),
      new QuicEpollConnectionHelper(epoll_server()));
}

void QuicTestServer::SetSessionCreator(SessionCreationFunction function) {
  static_cast<QuicTestDispatcher*>(dispatcher())
      ->set_session_creator(std::move(function));
}

void QuicTestServer::SetSpdyStreamCreator(StreamCreationFunction function) {
  DCHECK(dispatcher());
  static_cast<QuicTestDispatcher*>(dispatcher())
      ->set_stream_creator(std::move(function));
}

///////////////////////////   TEST SESSIONS ///////////////////////////////

ImmediateGoAwaySession::ImmediateGoAwaySession(
    const QuicConfig& config,
    QuicConnection* connection,
    QuicServerSessionVisitor* visitor,
    const QuicCryptoServerConfig* crypto_config)
    : QuicServerSession(config, connection, visitor, crypto_config) {
  SendGoAway(QUIC_PEER_GOING_AWAY, "");
}

}  // namespace test
}  // namespace tools
}  // namespace net
