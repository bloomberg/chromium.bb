// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_server_config.h"

#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"

namespace net {

SSLServerConfig::SSLServerConfig()
    : version_min(kDefaultSSLVersionMin),
      version_max(SSL_PROTOCOL_VERSION_TLS1_2),
      require_ecdhe(false),
      require_client_cert(false) {}

SSLServerConfig::~SSLServerConfig() {}

}  // namespace net
