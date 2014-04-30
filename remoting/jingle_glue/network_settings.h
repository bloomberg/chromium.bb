// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_NETWORK_SETTINGS_H_
#define REMOTING_JINGLE_GLUE_NETWORK_SETTINGS_H_

namespace remoting {

struct NetworkSettings {

  // When hosts are configured with NAT traversal disabled they will
  // typically also limit their P2P ports to this range, so that
  // sessions may be blocked or un-blocked via firewall rules.
  static const int kDefaultMinPort = 12400;
  static const int kDefaultMaxPort = 12409;

  enum NatTraversalMode {
    // Active NAT traversal using STUN and relay servers.
    NAT_TRAVERSAL_ENABLED,

    // Don't use STUN or relay servers. Accept incoming P2P connection
    // attempts, but don't initiate any. This ensures that the peer is
    // on the same network. Note that connection will always fail if
    // both ends use this mode.
    NAT_TRAVERSAL_DISABLED,

    // Don't use STUN or relay servers but make outgoing connections.
    NAT_TRAVERSAL_OUTGOING,
  };

  NetworkSettings()
      : nat_traversal_mode(NAT_TRAVERSAL_DISABLED),
        min_port(0),
        max_port(0) {
  }

  explicit NetworkSettings(NatTraversalMode nat_traversal_mode)
      : nat_traversal_mode(nat_traversal_mode),
        min_port(0),
        max_port(0) {
  }

  NatTraversalMode nat_traversal_mode;

  // |min_port| and |max_port| specify range (inclusive) of ports used by
  // P2P sessions. Any port can be used when both values are set to 0.
  int min_port;
  int max_port;
};

}  // namespace remoting

#endif  // REMOTING_HOST_NETWORK_SETTINGS_H_
