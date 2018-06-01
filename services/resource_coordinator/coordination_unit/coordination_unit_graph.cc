// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace resource_coordinator {

CoordinationUnitGraph::CoordinationUnitGraph() {
  CoordinationUnitBase::AssertNoActiveCoordinationUnits();
}

CoordinationUnitGraph::~CoordinationUnitGraph() {
  // TODO(oysteine): Keep the map of coordination units as a member of this
  // class, rather than statically inside CoordinationUnitBase, to avoid this
  // manual lifetime management.
  CoordinationUnitBase::ClearAllCoordinationUnits();
}

void CoordinationUnitGraph::OnStart(
    service_manager::BinderRegistryWithArgs<
        const service_manager::BindSourceInfo&>* registry,
    service_manager::ServiceContextRefFactory* service_ref_factory) {
  // Create the singleton CoordinationUnitProvider.
  provider_ =
      std::make_unique<CoordinationUnitProviderImpl>(service_ref_factory, this);
  registry->AddInterface(base::BindRepeating(
      &CoordinationUnitProviderImpl::Bind, base::Unretained(provider_.get())));

  // Create a singleton SystemCU instance. Ownership is taken by
  // CoordinationUnitBase. This interface is not directly registered to the
  // service, but rather clients can access it via CoordinationUnitProvider.
  CoordinationUnitID system_cu_id(CoordinationUnitType::kSystem, std::string());
  system_cu_ = SystemCoordinationUnitImpl::Create(
      system_cu_id, this, service_ref_factory->CreateRef());
}

void CoordinationUnitGraph::RegisterObserver(
    std::unique_ptr<CoordinationUnitGraphObserver> observer) {
  observer->set_coordination_unit_graph(this);
  observers_.push_back(std::move(observer));
}

void CoordinationUnitGraph::OnCoordinationUnitCreated(
    CoordinationUnitBase* coordination_unit) {
  for (auto& observer : observers_) {
    if (observer->ShouldObserve(coordination_unit)) {
      coordination_unit->AddObserver(observer.get());
      observer->OnCoordinationUnitCreated(coordination_unit);
    }
  }
}

void CoordinationUnitGraph::OnBeforeCoordinationUnitDestroyed(
    CoordinationUnitBase* coordination_unit) {
  coordination_unit->BeforeDestroyed();
}

FrameCoordinationUnitImpl* CoordinationUnitGraph::CreateFrameCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return FrameCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

PageCoordinationUnitImpl* CoordinationUnitGraph::CreatePageCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return PageCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

ProcessCoordinationUnitImpl*
CoordinationUnitGraph::CreateProcessCoordinationUnit(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  return ProcessCoordinationUnitImpl::Create(id, this, std::move(service_ref));
}

}  // namespace resource_coordinator
