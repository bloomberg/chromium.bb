// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  bool nat_traversal;
  std::string stun_server;
  std::string relay_server;
  std::string relay_token;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_TRANSPORT_CONFIG_H_
