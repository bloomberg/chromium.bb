// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/resource_coordinator_service.h"

#include <utility>

#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/observers/ipc_volume_reporter.h"
#include "services/resource_coordinator/observers/metrics_collector.h"
#include "services/resource_coordinator/observers/page_signal_generator_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

#if defined(OS_WIN)
#include "services/resource_coordinator/observers/working_set_trimmer_win.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorService::ResourceCoordinatorService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::nullopt /* idle_timeout */),
      introspector_(&coordination_unit_graph_) {}

ResourceCoordinatorService::~ResourceCoordinatorService() = default;

void ResourceCoordinatorService::OnStart() {
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(service_binding_.GetConnector());

  registry_.AddInterface(
      base::Bind(&CoordinationUnitIntrospectorImpl::BindToInterface,
                 base::Unretained(&introspector_)));

  // Register new |CoordinationUnitGraphObserver| implementations here.
  auto page_signal_generator_impl = std::make_unique<PageSignalGeneratorImpl>();
  registry_.AddInterface(
      base::Bind(&PageSignalGeneratorImpl::BindToInterface,
                 base::Unretained(page_signal_generator_impl.get())));
  coordination_unit_graph_.RegisterObserver(
      std::move(page_signal_generator_impl));

  coordination_unit_graph_.RegisterObserver(
      std::make_unique<MetricsCollector>());

  coordination_unit_graph_.RegisterObserver(std::make_unique<IPCVolumeReporter>(
      std::make_unique<base::OneShotTimer>()));

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kEmptyWorkingSet)) {
    coordination_unit_graph_.RegisterObserver(
        std::make_unique<WorkingSetTrimmer>());
  }
#endif

  coordination_unit_graph_.OnStart(&registry_, &service_keepalive_);
  coordination_unit_graph_.set_ukm_recorder(ukm_recorder_.get());

  // TODO(chiniforooshan): The abstract class Coordinator in the
  // public/cpp/memory_instrumentation directory should not be needed anymore.
  // We should be able to delete that and rename
  // memory_instrumentation::CoordinatorImpl to
  // memory_instrumentation::Coordinator.
  memory_instrumentation_coordinator_ =
      std::make_unique<memory_instrumentation::CoordinatorImpl>(
          service_binding_.GetConnector());
  registry_.AddInterface(base::BindRepeating(
      &memory_instrumentation::CoordinatorImpl::BindCoordinatorRequest,
      base::Unretained(memory_instrumentation_coordinator_.get())));
  registry_.AddInterface(base::BindRepeating(
      &memory_instrumentation::CoordinatorImpl::BindHeapProfilerHelperRequest,
      base::Unretained(memory_instrumentation_coordinator_.get())));
  registry_.AddInterface(base::BindRepeating(
      &ResourceCoordinatorService::BindWebUIGraphDump, base::Unretained(this)));
}

void ResourceCoordinatorService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

void ResourceCoordinatorService::BindWebUIGraphDump(
    mojom::WebUIGraphDumpRequest request,
    const service_manager::BindSourceInfo& source_info) {
  std::unique_ptr<WebUIGraphDumpImpl> graph_dump =
      std::make_unique<WebUIGraphDumpImpl>(&coordination_unit_graph_);

  auto error_callback =
      base::BindOnce(&ResourceCoordinatorService::OnGraphDumpConnectionError,
                     base::Unretained(this), graph_dump.get());
  graph_dump->Bind(std::move(request), std::move(error_callback));

  graph_dumps_.push_back(std::move(graph_dump));
}

void ResourceCoordinatorService::OnGraphDumpConnectionError(
    WebUIGraphDumpImpl* graph_dump) {
  const auto it = std::find_if(
      graph_dumps_.begin(), graph_dumps_.end(),
      [graph_dump](const std::unique_ptr<WebUIGraphDumpImpl>& graph_dump_ptr) {
        return graph_dump_ptr.get() == graph_dump;
      });

  DCHECK(it != graph_dumps_.end());

  graph_dumps_.erase(it);
}

}  // namespace resource_coordinator
