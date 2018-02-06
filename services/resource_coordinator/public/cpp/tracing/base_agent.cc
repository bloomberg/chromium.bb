// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/tracing/base_agent.h"

#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace tracing {

BaseAgent::BaseAgent(service_manager::Connector* connector,
                     const std::string& label,
                     mojom::TraceDataType type,
                     bool supports_explicit_clock_sync)
    : binding_(this) {
  // |connector| can be null in tests.
  if (!connector)
    return;
  tracing::mojom::AgentRegistryPtr agent_registry;
  connector->BindInterface(resource_coordinator::mojom::kServiceName,
                           &agent_registry);

  tracing::mojom::AgentPtr agent;
  binding_.Bind(mojo::MakeRequest(&agent));
  agent_registry->RegisterAgent(std::move(agent), label, type,
                                supports_explicit_clock_sync);
}

BaseAgent::~BaseAgent() = default;

void BaseAgent::StartTracing(const std::string& config,
                             base::TimeTicks coordinator_time,
                             const Agent::StartTracingCallback& callback) {
  callback.Run(true /* success */);
}

void BaseAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {}

void BaseAgent::RequestClockSyncMarker(
    const std::string& sync_id,
    const Agent::RequestClockSyncMarkerCallback& callback) {
  NOTREACHED() << "The agent claims to support explicit clock sync but does "
               << "not override BaseAgent::RequestClockSyncMarker()";
}

void BaseAgent::GetCategories(const Agent::GetCategoriesCallback& callback) {
  callback.Run("" /* categories */);
}

void BaseAgent::RequestBufferStatus(
    const Agent::RequestBufferStatusCallback& callback) {
  callback.Run(0 /* capacity */, 0 /* count */);
}

}  // namespace tracing
