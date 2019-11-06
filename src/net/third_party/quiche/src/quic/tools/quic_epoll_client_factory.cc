// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/tools/quic_epoll_client_factory.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quic/tools/quic_client.h"

namespace quic {

std::unique_ptr<QuicSpdyClientBase> QuicEpollClientFactory::CreateClient(
    std::string host_for_handshake,
    std::string host_for_lookup,
    uint16_t port,
    ParsedQuicVersionVector versions,
    std::unique_ptr<ProofVerifier> verifier) {
  QuicSocketAddress addr =
      tools::LookupAddress(host_for_lookup, QuicStrCat(port));
  if (!addr.IsInitialized()) {
    QUIC_LOG(ERROR) << "Unable to resolve address: " << host_for_lookup;
    return nullptr;
  }
  QuicServerId server_id(host_for_handshake, port, false);
  return QuicMakeUnique<QuicClient>(addr, server_id, versions, &epoll_server_,
                                    std::move(verifier));
}

}  // namespace quic
