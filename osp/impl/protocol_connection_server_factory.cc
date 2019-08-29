// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/public/protocol_connection_server_factory.h"

#include <memory>

#include "osp/impl/quic/quic_connection_factory_impl.h"
#include "osp/impl/quic/quic_server.h"
#include "osp/public/network_service_manager.h"
#include "platform/api/task_runner.h"

namespace openscreen {

// static
std::unique_ptr<ProtocolConnectionServer>
ProtocolConnectionServerFactory::Create(
    const ServerConfig& config,
    MessageDemuxer* demuxer,
    ProtocolConnectionServer::Observer* observer,
    platform::TaskRunner* task_runner) {
  return std::make_unique<QuicServer>(
      config, demuxer, std::make_unique<QuicConnectionFactoryImpl>(task_runner),
      observer, task_runner);
}

}  // namespace openscreen
