// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <utility>

#include "base/timer/timer.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/public/cpp/tracing_features.h"

#if defined(PERFETTO_SERVICE_AVAILABLE)
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"
#endif

namespace tracing {

TracingService::TracingService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

TracingService::~TracingService() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(tracing_agent_registry_));

#if defined(PERFETTO_SERVICE_AVAILABLE)
  if (perfetto_tracing_coordinator_) {
    task_runner_->DeleteSoon(FROM_HERE,
                             std::move(perfetto_tracing_coordinator_));
  }
#endif
}

void TracingService::OnStart() {
  tracing_agent_registry_ = std::make_unique<AgentRegistry>();

  bool enable_legacy_tracing = true;

#if defined(PERFETTO_SERVICE_AVAILABLE)
  registry_.AddInterface(base::BindRepeating(
      &tracing::PerfettoService::BindRequest,
      base::Unretained(tracing::PerfettoService::GetInstance())));

  if (TracingUsesPerfettoBackend()) {
    task_runner_ = tracing::PerfettoService::GetInstance()->task_runner();

    auto perfetto_coordinator = std::make_unique<PerfettoTracingCoordinator>(
        tracing_agent_registry_.get());
    registry_.AddInterface(
        base::BindRepeating(&PerfettoTracingCoordinator::BindCoordinatorRequest,
                            base::Unretained(perfetto_coordinator.get())));
    perfetto_tracing_coordinator_ = std::move(perfetto_coordinator);

    enable_legacy_tracing = false;
  }
#else
  if (TracingUsesPerfettoBackend()) {
    LOG(ERROR) << "Perfetto is not yet available for this platform; falling "
                  "back to using legacy TraceLog";
  }
#endif

  // Use legacy tracing if we're on an unsupported platform or the feature flag
  // is disabled.
  if (enable_legacy_tracing) {
    auto tracing_coordinator =
        std::make_unique<Coordinator>(tracing_agent_registry_.get());
    registry_.AddInterface(
        base::BindRepeating(&Coordinator::BindCoordinatorRequest,
                            base::Unretained(tracing_coordinator.get())));
    tracing_coordinator_ = std::move(tracing_coordinator);
  }

  registry_.AddInterface(base::BindRepeating(
      &AgentRegistry::BindAgentRegistryRequest,
      base::Unretained(tracing_agent_registry_.get()), task_runner_));
}

void TracingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

}  // namespace tracing
