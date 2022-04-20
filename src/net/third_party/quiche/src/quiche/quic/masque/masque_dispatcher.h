// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_MASQUE_MASQUE_DISPATCHER_H_
#define QUICHE_QUIC_MASQUE_MASQUE_DISPATCHER_H_

#include "absl/container/flat_hash_map.h"
#include "quiche/quic/masque/masque_server_backend.h"
#include "quiche/quic/masque/masque_server_session.h"
#include "quiche/quic/masque/masque_utils.h"
#include "quiche/quic/platform/api/quic_epoll.h"
#include "quiche/quic/platform/api/quic_export.h"
#include "quiche/quic/tools/quic_simple_dispatcher.h"

namespace quic {

// QUIC dispatcher that handles new MASQUE connections and can proxy traffic
// between MASQUE clients and QUIC servers.
class QUIC_NO_EXPORT MasqueDispatcher : public QuicSimpleDispatcher,
                                        public MasqueServerSession::Visitor {
 public:
  explicit MasqueDispatcher(
      MasqueMode masque_mode, const QuicConfig* config,
      const QuicCryptoServerConfig* crypto_config,
      QuicVersionManager* version_manager, QuicEpollServer* epoll_server,
      std::unique_ptr<QuicConnectionHelperInterface> helper,
      std::unique_ptr<QuicCryptoServerStreamBase::Helper> session_helper,
      std::unique_ptr<QuicAlarmFactory> alarm_factory,
      MasqueServerBackend* masque_server_backend,
      uint8_t expected_server_connection_id_length);

  // Disallow copy and assign.
  MasqueDispatcher(const MasqueDispatcher&) = delete;
  MasqueDispatcher& operator=(const MasqueDispatcher&) = delete;

  // From QuicSimpleDispatcher.
  std::unique_ptr<QuicSession> CreateQuicSession(
      QuicConnectionId connection_id, const QuicSocketAddress& self_address,
      const QuicSocketAddress& peer_address, absl::string_view alpn,
      const ParsedQuicVersion& version,
      const quic::ParsedClientHello& parsed_chlo) override;

  bool OnFailedToDispatchPacket(const ReceivedPacketInfo& packet_info) override;

  // From MasqueServerSession::Visitor.
  void RegisterClientConnectionId(
      QuicConnectionId client_connection_id,
      MasqueServerSession* masque_server_session) override;

  void UnregisterClientConnectionId(
      QuicConnectionId client_connection_id) override;

 private:
  MasqueMode masque_mode_;
  QuicEpollServer* epoll_server_;               // Unowned.
  MasqueServerBackend* masque_server_backend_;  // Unowned.
  // Mapping from client connection IDs to server sessions, allows routing
  // incoming packets to the right MASQUE connection.
  absl::flat_hash_map<QuicConnectionId, MasqueServerSession*,
                      QuicConnectionIdHash>
      client_connection_id_registrations_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_MASQUE_MASQUE_DISPATCHER_H_
