// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/p2p_transport.h"

namespace webkit_glue {

P2PTransport::Config::Config()
    : stun_server_port(0),
      relay_server_port(0),
      tcp_receive_window(0),
      tcp_send_window(0),
      tcp_no_delay(false),
      tcp_ack_delay_ms(0) {
}

P2PTransport::Config::~Config() {
}

}  // namespace webkit_glue
