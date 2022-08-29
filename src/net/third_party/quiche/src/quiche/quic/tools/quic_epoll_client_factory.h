// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_TOOLS_QUIC_EPOLL_CLIENT_FACTORY_H_
#define QUICHE_QUIC_TOOLS_QUIC_EPOLL_CLIENT_FACTORY_H_

#include "quiche/quic/platform/api/quic_epoll.h"
#include "quiche/quic/tools/quic_toy_client.h"

namespace quic {

// Factory creating QuicClient instances.
class QuicEpollClientFactory : public QuicToyClient::ClientFactory {
 public:
  std::unique_ptr<QuicSpdyClientBase> CreateClient(
      std::string host_for_handshake, std::string host_for_lookup,
      int address_family_for_lookup, uint16_t port,
      ParsedQuicVersionVector versions, const QuicConfig& config,
      std::unique_ptr<ProofVerifier> verifier,
      std::unique_ptr<SessionCache> session_cache) override;

 private:
  QuicEpollServer epoll_server_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_TOOLS_QUIC_EPOLL_CLIENT_FACTORY_H_
