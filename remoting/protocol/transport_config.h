// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_TRANSPORT_CONFIG_H_
#define REMOTING_PROTOCOL_TRANSPORT_CONFIG_H_

#include <string>

namespace remoting {
namespace protocol {

struct TransportConfig {
  TransportConfig();
  ~TransportConfig();

  enum NatTraversalMode {
    // Don't use STUN or relay servers. Accept incoming P2P connection
    // attempts, but don't initiate any. This ensures that the peer is
    // on the same network. Note that connection will always fail if
    // both ends use this mode.
    NAT_TRAVERSAL_DISABLED,

    // Don't use STUN or relay servers but make outgoing connections.
    NAT_TRAVERSAL_OUTGOING,

    // Active NAT traversal using STUN and relay servers.
    NAT_TRAVERSAL_ENABLED,
  };

  NatTraversalMode nat_traversal_mode;
  std::string stun_server;
  std::string relay_server;
  std::string relay_token;

  // |min_port| and |max_port| specify range (inclusive) of ports used by
  // P2P sessions. Any port can be used when both values are set to 0.
  int min_port;
  int max_port;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONFIG_H_
