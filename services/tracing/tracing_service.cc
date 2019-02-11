// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

// Listener used to connect to every other service and
// pass them the needed interface pointers to connect
// back and register with the tracing service.
class ServiceListener : public service_manager::mojom::ServiceManagerListener {
 public:
  ServiceListener(service_manager::Connector* connector,
                  AgentRegistry* agent_registry)
      : binding_(this),
        connector_(connector),
        agent_registry_(agent_registry) {
    service_manager::mojom::ServiceManagerPtr service_manager;
    connector_->BindInterface(service_manager::mojom::kServiceName,
                              &service_manager);
    service_manager::mojom::ServiceManagerListenerPtr listener;
    service_manager::mojom::ServiceManagerListenerRequest request(
        mojo::MakeRequest(&listener));
    service_manager->AddListener(std::move(listener));
    binding_.Bind(std::move(request));
  }

  void ConnectProcessToTracingService(
      const service_manager::Identity& identity) {
    mojom::TracedProcessPtr traced_process;
    connector_->BindInterface(
        service_manager::ServiceFilter::ForExactIdentity(identity),
        mojo::MakeRequest(&traced_process),
        service_manager::mojom::BindInterfacePriority::kBestEffort);

    auto new_connection_request = mojom::ConnectToTracingRequest::New();

    PerfettoService::GetInstance()->BindRequest(
        mojo::MakeRequest(&new_connection_request->perfetto_service));

    agent_registry_->BindAgentRegistryRequest(
        mojo::MakeRequest(&new_connection_request->agent_registry));

    traced_process->ConnectToTracingService(std::move(new_connection_request));
  }

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override {
    for (auto& service : running_services) {
      ConnectProcessToTracingService(service->identity);
    }
  }

  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
    ConnectProcessToTracingService(identity);
  }

  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override {}
  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {}
  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {}
  void OnServiceStopped(const service_manager::Identity& identity) override {}

 private:
  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_;
  service_manager::Connector* connector_;
  AgentRegistry* agent_registry_;
};

TracingService::TracingService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)) {}

TracingService::~TracingService() = default;

void TracingService::OnDisconnected() {
  CloseAgentConnectionsAndTerminate();
}

void TracingService::OnStart() {
  tracing_agent_registry_ = std::make_unique<AgentRegistry>();

  if (TracingUsesPerfettoBackend()) {
    auto perfetto_coordinator = std::make_unique<PerfettoTracingCoordinator>(
        tracing_agent_registry_.get(),
        base::BindRepeating(&TracingService::OnCoordinatorConnectionClosed,
                            base::Unretained(this)));
    registry_.AddInterface(
        base::BindRepeating(&PerfettoTracingCoordinator::BindCoordinatorRequest,
                            base::Unretained(perfetto_coordinator.get())));
    perfetto_tracing_coordinator_ = std::move(perfetto_coordinator);
  } else {
    auto tracing_coordinator = std::make_unique<Coordinator>(
        tracing_agent_registry_.get(),
        base::BindRepeating(&TracingService::OnCoordinatorConnectionClosed,
                            base::Unretained(this)));
    registry_.AddInterface(
        base::BindRepeating(&Coordinator::BindCoordinatorRequest,
                            base::Unretained(tracing_coordinator.get())));
    tracing_coordinator_ = std::move(tracing_coordinator);
  }

  service_listener_ = std::make_unique<ServiceListener>(
      service_binding_.GetConnector(), tracing_agent_registry_.get());
}

void TracingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

void TracingService::OnCoordinatorConnectionClosed() {
  service_binding_.RequestClose();
}

void TracingService::CloseAgentConnectionsAndTerminate() {
  tracing_agent_registry_->DisconnectAllAgents();
  Terminate();
}

}  // namespace tracing
