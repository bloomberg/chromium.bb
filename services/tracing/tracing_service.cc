// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <utility>

#include "base/timer/timer.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"

namespace tracing {

std::unique_ptr<service_manager::Service> TracingService::Create() {
  return std::make_unique<TracingService>();
}

TracingService::TracingService() : weak_factory_(this) {}

TracingService::~TracingService() = default;

void TracingService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      context()->CreateQuitClosure()));

  tracing_agent_registry_ = std::make_unique<AgentRegistry>(ref_factory_.get());
  registry_.AddInterface(
      base::BindRepeating(&AgentRegistry::BindAgentRegistryRequest,
                          base::Unretained(tracing_agent_registry_.get())));

  tracing_coordinator_ = std::make_unique<Coordinator>(ref_factory_.get());
  registry_.AddInterface(
      base::BindRepeating(&Coordinator::BindCoordinatorRequest,
                          base::Unretained(tracing_coordinator_.get())));
}

void TracingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

}  // namespace tracing
