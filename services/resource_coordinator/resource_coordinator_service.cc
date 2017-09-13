// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/observers/metrics_collector.h"
#include "services/resource_coordinator/observers/tab_signal_generator_impl.h"
#include "services/resource_coordinator/service_callbacks_impl.h"
#include "services/resource_coordinator/tracing/agent_registry.h"
#include "services/resource_coordinator/tracing/coordinator.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace resource_coordinator {

std::unique_ptr<service_manager::Service> ResourceCoordinatorService::Create() {
  auto resource_coordinator_service =
      base::MakeUnique<ResourceCoordinatorService>();

  return resource_coordinator_service;
}

ResourceCoordinatorService::ResourceCoordinatorService()
    : weak_factory_(this) {}

ResourceCoordinatorService::~ResourceCoordinatorService() {
  ref_factory_.reset();
}

void ResourceCoordinatorService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));

  registry_.AddInterface(base::Bind(ServiceCallbacksImpl::Create,
                                    base::Unretained(ref_factory_.get()),
                                    base::Unretained(this)));

  registry_.AddInterface(
      base::Bind(&CoordinationUnitIntrospectorImpl::BindToInterface,
                 base::Unretained(&introspector_)));

  // Register new |CoordinationUnitGraphObserver| implementations here.
  auto tab_signal_generator_impl = base::MakeUnique<TabSignalGeneratorImpl>();
  registry_.AddInterface(
      base::Bind(&TabSignalGeneratorImpl::BindToInterface,
                 base::Unretained(tab_signal_generator_impl.get())));
  coordination_unit_manager_.RegisterObserver(
      std::move(tab_signal_generator_impl));

  coordination_unit_manager_.RegisterObserver(
      base::MakeUnique<MetricsCollector>());

  coordination_unit_manager_.OnStart(&registry_, ref_factory_.get());

  // TODO(chiniforooshan): The abstract class Coordinator in the
  // public/cpp/memory_instrumentation directory should not be needed anymore.
  // We should be able to delete that and rename
  // memory_instrumentation::CoordinatorImpl to
  // memory_instrumentation::Coordinator.
  memory_instrumentation_coordinator_ =
      base::MakeUnique<memory_instrumentation::CoordinatorImpl>(
          context()->connector());
  registry_.AddInterface(base::BindRepeating(
      &memory_instrumentation::CoordinatorImpl::BindCoordinatorRequest,
      base::Unretained(memory_instrumentation_coordinator_.get())));

  tracing_agent_registry_ = base::MakeUnique<tracing::AgentRegistry>();
  registry_.AddInterface(
      base::BindRepeating(&tracing::AgentRegistry::BindAgentRegistryRequest,
                          base::Unretained(tracing_agent_registry_.get())));

  tracing_coordinator_ = base::MakeUnique<tracing::Coordinator>();
  registry_.AddInterface(
      base::BindRepeating(&tracing::Coordinator::BindCoordinatorRequest,
                          base::Unretained(tracing_coordinator_.get())));
}

void ResourceCoordinatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

void ResourceCoordinatorService::SetUkmRecorder(
    std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder) {
  ukm_recorder_ = std::move(ukm_recorder);
  coordination_unit_manager_.set_ukm_recorder(ukm_recorder_.get());
}

}  // namespace resource_coordinator
