// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_
#define API_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_

#include <memory>

#include "api/public/protocol_connection_server.h"
#include "api/public/server_config.h"

namespace openscreen {

class ProtocolConnectionServerFactory {
 public:
  static std::unique_ptr<ProtocolConnectionServer> Create(
      const ServerConfig& config,
      MessageDemuxer* demuxer,
      ProtocolConnectionServer::Observer* observer);
};

}  // namespace openscreen

#endif  // API_PUBLIC_PROTOCOL_CONNECTION_SERVER_FACTORY_H_
