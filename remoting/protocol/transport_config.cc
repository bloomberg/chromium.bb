// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_config.h"

namespace remoting {
namespace protocol {

TransportConfig::TransportConfig()
    : nat_traversal_mode(NAT_TRAVERSAL_DISABLED),
      min_port(0),
      max_port(0) {
}

TransportConfig::~TransportConfig() {
}

}  // namespace protocol
}  // namespace remoting
