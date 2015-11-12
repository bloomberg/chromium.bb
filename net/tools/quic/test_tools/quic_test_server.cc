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
  CustomStreamSession(
      const QuicConfig& config,
      QuicConnection* connection,
      QuicServerSessionVisitor* visitor,
      const QuicCryptoServerConfig* crypto_config,
      QuicTestServer::StreamFactory* factory,
      QuicTestServer::CryptoStreamFactory* crypto_stream_factory)
      : QuicServerSession(config, connection, visitor, crypto_config),
        stream_factory_(factory),
        crypto_stream_factory_(crypto_stream_factory) {}

  QuicSpdyStream* CreateIncomingDynamicStream(QuicStreamId id) override {
    if (!ShouldCreateIncomingDynamicStream(id)) {
      return nullptr;
    }
    if (stream_factory_) {
      return stream_factory_->CreateStream(id, this);
    }
    return QuicServerSession::CreateIncomingDynamicStream(id);
  }

  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config) override {
    if (crypto_stream_factory_) {
      return crypto_stream_factory_->CreateCryptoStream(crypto_config, this);
    }
    return QuicServerSession::CreateQuicCryptoServerStream(crypto_config);
  }

 private:
  QuicTestServer::StreamFactory* stream_factory_;               // Not owned.
  QuicTestServer::CryptoStreamFactory* crypto_stream_factory_;  // Not owned.
};

class QuicTestDispatcher : public QuicDispatcher {
 public:
  QuicTestDispatcher(const QuicConfig& config,
                     const QuicCryptoServerConfig* crypto_config,
                     const QuicVersionVector& versions,
                     PacketWriterFactory* factory,
                     QuicConnectionHelperInterface* helper)
      : QuicDispatcher(config, crypto_config, versions, factory, helper),
        session_factory_(nullptr),
        stream_factory_(nullptr),
        crypto_stream_factory_(nullptr) {}

  QuicServerSession* CreateQuicSession(QuicConnectionId id,
                                       const IPEndPoint& client) override {
    if (session_factory_ == nullptr && stream_factory_ == nullptr &&
        crypto_stream_factory_ == nullptr) {
      return QuicDispatcher::CreateQuicSession(id, client);
    }
    QuicConnection* connection = new QuicConnection(
        id, client, helper(), connection_writer_factory(),
        /* owns_writer= */ true, Perspective::IS_SERVER, supported_versions());

    QuicServerSession* session = nullptr;
    if (stream_factory_ != nullptr || crypto_stream_factory_ != nullptr) {
      session =
          new CustomStreamSession(config(), connection, this, crypto_config(),
                                  stream_factory_, crypto_stream_factory_);
    } else {
      session = session_factory_->CreateSession(config(), connection, this,
                                                crypto_config());
    }
    session->Initialize();
    return session;
  }

  void set_session_factory(QuicTestServer::SessionFactory* factory) {
    DCHECK(session_factory_ == nullptr);
    DCHECK(stream_factory_ == nullptr);
    DCHECK(crypto_stream_factory_ == nullptr);
    session_factory_ = factory;
  }

  void set_stream_factory(QuicTestServer::StreamFactory* factory) {
    DCHECK(session_factory_ == nullptr);
    DCHECK(stream_factory_ == nullptr);
    stream_factory_ = factory;
  }

  void set_crypto_stream_factory(QuicTestServer::CryptoStreamFactory* factory) {
    DCHECK(session_factory_ == nullptr);
    DCHECK(crypto_stream_factory_ == nullptr);
    crypto_stream_factory_ = factory;
  }

  QuicTestServer::SessionFactory* session_factory() { return session_factory_; }

  QuicTestServer::StreamFactory* stream_factory() { return stream_factory_; }

 private:
  QuicTestServer::SessionFactory* session_factory_;             // Not owned.
  QuicTestServer::StreamFactory* stream_factory_;               // Not owned.
  QuicTestServer::CryptoStreamFactory* crypto_stream_factory_;  // Not owned.
};

QuicTestServer::QuicTestServer(ProofSource* proof_source)
    : QuicServer(proof_source) {}

QuicTestServer::QuicTestServer(ProofSource* proof_source,
                               const QuicConfig& config,
                               const QuicVersionVector& supported_versions)
    : QuicServer(proof_source, config, supported_versions) {}

QuicDispatcher* QuicTestServer::CreateQuicDispatcher() {
  return new QuicTestDispatcher(
      config(), &crypto_config(), supported_versions(),
      new QuicDispatcher::DefaultPacketWriterFactory(),
      new QuicEpollConnectionHelper(epoll_server()));
}

void QuicTestServer::SetSessionFactory(SessionFactory* factory) {
  DCHECK(dispatcher());
  static_cast<QuicTestDispatcher*>(dispatcher())->set_session_factory(factory);
}

void QuicTestServer::SetSpdyStreamFactory(StreamFactory* factory) {
  static_cast<QuicTestDispatcher*>(dispatcher())->set_stream_factory(factory);
}

void QuicTestServer::SetCryptoStreamFactory(CryptoStreamFactory* factory) {
  static_cast<QuicTestDispatcher*>(dispatcher())
      ->set_crypto_stream_factory(factory);
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
