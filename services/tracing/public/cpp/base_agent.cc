// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/base_agent.h"

#include <utility>

#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace tracing {

BaseAgent::BaseAgent(const std::string& label,
                     mojom::TraceDataType type,
                     base::ProcessId pid)
    : binding_(this), label_(label), type_(type), pid_(pid) {}

BaseAgent::~BaseAgent() = default;

void BaseAgent::Connect(service_manager::Connector* connector) {
  DCHECK(!binding_ || !binding_.is_bound());
  tracing::mojom::AgentRegistryPtr agent_registry;
  connector->BindInterface(tracing::mojom::kServiceName, &agent_registry);

  tracing::mojom::AgentPtr agent;
  binding_.Bind(mojo::MakeRequest(&agent));
  agent_registry->RegisterAgent(std::move(agent), label_, type_, pid_);
}

void BaseAgent::GetCategories(std::set<std::string>* category_set) {}

void BaseAgent::StartTracing(const std::string& config,
                             base::TimeTicks coordinator_time,
                             Agent::StartTracingCallback callback) {
  std::move(callback).Run(true /* success */);
}

void BaseAgent::StopAndFlush(tracing::mojom::RecorderPtr recorder) {}

void BaseAgent::RequestBufferStatus(
    Agent::RequestBufferStatusCallback callback) {
  std::move(callback).Run(0 /* capacity */, 0 /* count */);
}

}  // namespace tracing
