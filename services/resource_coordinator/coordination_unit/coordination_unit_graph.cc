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

CoordinationUnitGraph::CoordinationUnitGraph()
    : system_coordination_unit_id_(CoordinationUnitType::kSystem,
                                   CoordinationUnitID::RANDOM_ID) {}

CoordinationUnitGraph::~CoordinationUnitGraph() = default;

void CoordinationUnitGraph::OnStart(
    service_manager::BinderRegistryWithArgs<
        const service_manager::BindSourceInfo&>* registry,
    service_manager::ServiceContextRefFactory* service_ref_factory) {
  // Create the singleton CoordinationUnitProvider.
  provider_ =
      std::make_unique<CoordinationUnitProviderImpl>(service_ref_factory, this);
  registry->AddInterface(base::BindRepeating(
      &CoordinationUnitProviderImpl::Bind, base::Unretained(provider_.get())));
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

SystemCoordinationUnitImpl*
CoordinationUnitGraph::FindOrCreateSystemCoordinationUnit(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref) {
  CoordinationUnitBase* system_cu =
      GetCoordinationUnitByID(system_coordination_unit_id_);
  if (system_cu)
    return SystemCoordinationUnitImpl::FromCoordinationUnitBase(system_cu);

  // Create the singleton SystemCU instance. Ownership is taken by the graph.
  return SystemCoordinationUnitImpl::Create(system_coordination_unit_id_, this,
                                            std::move(service_ref));
}

CoordinationUnitBase* CoordinationUnitGraph::GetCoordinationUnitByID(
    const CoordinationUnitID cu_id) {
  const auto& it = coordination_units_.find(cu_id);
  if (it == coordination_units_.end())
    return nullptr;
  return it->second.get();
}

std::vector<CoordinationUnitBase*>
CoordinationUnitGraph::GetCoordinationUnitsOfType(CoordinationUnitType type) {
  std::vector<CoordinationUnitBase*> results;
  for (const auto& el : coordination_units_) {
    if (el.first.type == type)
      results.push_back(el.second.get());
  }
  return results;
}

std::vector<ProcessCoordinationUnitImpl*>
CoordinationUnitGraph::GetAllProcessCoordinationUnits() {
  auto cus = GetCoordinationUnitsOfType(CoordinationUnitType::kProcess);
  std::vector<ProcessCoordinationUnitImpl*> process_cus;
  for (auto* process_cu : cus) {
    process_cus.push_back(
        ProcessCoordinationUnitImpl::FromCoordinationUnitBase(process_cu));
  }
  return process_cus;
}

CoordinationUnitBase* CoordinationUnitGraph::AddNewCoordinationUnit(
    std::unique_ptr<CoordinationUnitBase> new_cu) {
  auto it = coordination_units_.emplace(new_cu->id(), std::move(new_cu));
  DCHECK(it.second);  // Inserted successfully

  CoordinationUnitBase* added_cu = it.first->second.get();
  OnCoordinationUnitCreated(added_cu);

  return added_cu;
}

void CoordinationUnitGraph::DestroyCoordinationUnit(CoordinationUnitBase* cu) {
  OnBeforeCoordinationUnitDestroyed(cu);

  size_t erased = coordination_units_.erase(cu->id());
  DCHECK_EQ(1u, erased);
}

}  // namespace resource_coordinator
