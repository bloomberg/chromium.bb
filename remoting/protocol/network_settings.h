// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NETWORK_SETTINGS_H_
#define REMOTING_PROTOCOL_NETWORK_SETTINGS_H_

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"

namespace remoting {
namespace protocol {

struct NetworkSettings {

  // When hosts are configured with NAT traversal disabled they will
  // typically also limit their P2P ports to this range, so that
  // sessions may be blocked or un-blocked via firewall rules.
  static const int kDefaultMinPort = 12400;
  static const int kDefaultMaxPort = 12409;

  enum Flags {
    // Don't use STUN or relay servers. Accept incoming P2P connection
    // attempts, but don't initiate any. This ensures that the peer is
    // on the same network. Note that connection will always fail if
    // both ends use this mode.
    NAT_TRAVERSAL_DISABLED = 0x0,

    // Allow outgoing connections, even when STUN and RELAY are not enabled.
    NAT_TRAVERSAL_OUTGOING = 0x1,

    // Active NAT traversal using STUN.
    NAT_TRAVERSAL_STUN = 0x2,

    // Allow the use of relay servers when a direct connection is not available.
    NAT_TRAVERSAL_RELAY = 0x4,

    // Active NAT traversal using STUN and relay servers.
    NAT_TRAVERSAL_FULL = NAT_TRAVERSAL_STUN | NAT_TRAVERSAL_RELAY |
        NAT_TRAVERSAL_OUTGOING
  };

  NetworkSettings()
      : flags(NAT_TRAVERSAL_DISABLED),
        min_port(0),
        max_port(0) {
    DCHECK(!(flags & (NAT_TRAVERSAL_STUN | NAT_TRAVERSAL_RELAY)) ||
           (flags & NAT_TRAVERSAL_OUTGOING));
  }

  explicit NetworkSettings(uint32 flags)
      : flags(flags),
        min_port(0),
        max_port(0) {
  }

  // Parse string in the form "<min_port>-<max_port>". E.g. "12400-12409".
  // Returns true if string was parsed successfuly.
  static bool ParsePortRange(const std::string& port_range,
                             int* out_min_port,
                             int* out_max_port);

  uint32 flags;

  // |min_port| and |max_port| specify range (inclusive) of ports used by
  // P2P sessions. Any port can be used when both values are set to 0.
  int min_port;
  int max_port;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_HOST_NETWORK_SETTINGS_H_
