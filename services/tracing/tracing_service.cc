// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/timer/timer.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/perfetto/consumer_host.h"
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
                  AgentRegistry* agent_registry,
                  Coordinator* coordinator)
      : binding_(this),
        connector_(connector),
        agent_registry_(agent_registry),
        coordinator_(coordinator) {
    service_manager::mojom::ServiceManagerPtr service_manager;
    connector_->BindInterface(service_manager::mojom::kServiceName,
                              &service_manager);
    service_manager::mojom::ServiceManagerListenerPtr listener;
    service_manager::mojom::ServiceManagerListenerRequest request(
        mojo::MakeRequest(&listener));
    service_manager->AddListener(std::move(listener));
    binding_.Bind(std::move(request));
  }

  size_t CountServicesWithPID(uint32_t pid) {
    return std::count_if(service_pid_map_.begin(), service_pid_map_.end(),
                         [pid](decltype(service_pid_map_)::value_type p) {
                           return p.second == pid;
                         });
  }

  void ServiceAddedWithPID(const service_manager::Identity& identity,
                           uint32_t pid) {
    service_pid_map_[identity] = pid;
    // Not the first service added, so we're already sent it a connection
    // request.
    if (CountServicesWithPID(pid) > 1) {
      return;
    }

    // Let the Coordinator know it should be expecting a connection
    // from this process.
    coordinator_->AddExpectedPID(pid);

    mojom::TracedProcessPtr traced_process;
    connector_->BindInterface(
        service_manager::ServiceFilter::ForExactIdentity(identity),
        mojo::MakeRequest(&traced_process),
        service_manager::mojom::BindInterfacePriority::kBestEffort);

    auto new_connection_request = mojom::ConnectToTracingRequest::New();

    PerfettoService::GetInstance()->BindRequest(
        mojo::MakeRequest(&new_connection_request->perfetto_service), pid);

    agent_registry_->BindAgentRegistryRequest(
        mojo::MakeRequest(&new_connection_request->agent_registry));

    traced_process->ConnectToTracingService(std::move(new_connection_request));
  }

  void ServiceRemoved(const service_manager::Identity& identity) {
    auto entry = service_pid_map_.find(identity);
    if (entry != service_pid_map_.end()) {
      uint32_t pid = entry->second;
      service_pid_map_.erase(entry);
      // Last entry with this PID removed; stop expecting it
      // to connect to the tracing service.
      if (CountServicesWithPID(pid) == 0) {
        coordinator_->RemoveExpectedPID(pid);
      }
    }
  }

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override {
    for (auto& service : running_services) {
      if (service->pid) {
        ServiceAddedWithPID(service->identity, service->pid);
      }
    }

    coordinator_->FinishedReceivingRunningPIDs();
  }

  void OnServicePIDReceived(const service_manager::Identity& identity,
                            uint32_t pid) override {
    ServiceAddedWithPID(identity, pid);
  }

  void OnServiceFailedToStart(
      const service_manager::Identity& identity) override {
    ServiceRemoved(identity);
  }

  void OnServiceStopped(const service_manager::Identity& identity) override {
    ServiceRemoved(identity);
  }

  void OnServiceStarted(const service_manager::Identity& identity,
                        uint32_t pid) override {
  }

  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override {}

 private:
  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_;
  service_manager::Connector* connector_;
  AgentRegistry* agent_registry_;
  Coordinator* coordinator_;
  std::map<service_manager::Identity, uint32_t> service_pid_map_;
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
    tracing_coordinator_ = std::move(perfetto_coordinator);
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

  registry_.AddInterface(
      base::BindRepeating(&ConsumerHost::BindConsumerRequest,
                          base::Unretained(PerfettoService::GetInstance())));

  service_listener_ = std::make_unique<ServiceListener>(
      service_binding_.GetConnector(), tracing_agent_registry_.get(),
      tracing_coordinator_.get());
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
