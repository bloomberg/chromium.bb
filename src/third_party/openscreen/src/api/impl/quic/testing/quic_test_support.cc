// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/quic_test_support.h"

#include <memory>

#include "api/impl/quic/quic_client.h"
#include "api/impl/quic/quic_server.h"
#include "api/public/network_service_manager.h"

namespace openscreen {

FakeQuicBridge::FakeQuicBridge()
    : initial_clock_time(platform::TimeDelta::FromMilliseconds(1298424)) {
  fake_bridge =
      std::make_unique<FakeQuicConnectionFactoryBridge>(kControllerEndpoint);

  controller_demuxer = std::make_unique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit,
      std::make_unique<FakeClock>(initial_clock_time));
  receiver_demuxer = std::make_unique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit,
      std::make_unique<FakeClock>(initial_clock_time));

  auto fake_client_factory =
      std::make_unique<FakeClientQuicConnectionFactory>(fake_bridge.get());
  quic_client = std::make_unique<QuicClient>(controller_demuxer.get(),
                                             std::move(fake_client_factory),
                                             &mock_client_observer);

  auto fake_server_factory =
      std::make_unique<FakeServerQuicConnectionFactory>(fake_bridge.get());
  ServerConfig config;
  config.connection_endpoints.push_back(kReceiverEndpoint);
  quic_server = std::make_unique<QuicServer>(config, receiver_demuxer.get(),
                                             std::move(fake_server_factory),
                                             &mock_server_observer);

  quic_client->Start();
  quic_server->Start();
}

FakeQuicBridge::~FakeQuicBridge() = default;

void FakeQuicBridge::RunTasksUntilIdle() {
  do {
    NetworkServiceManager::Get()->GetProtocolConnectionClient()->RunTasks();
    NetworkServiceManager::Get()->GetProtocolConnectionServer()->RunTasks();
  } while (!fake_bridge->idle());
}

}  // namespace openscreen
