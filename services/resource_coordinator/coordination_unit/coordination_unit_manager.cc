// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace ukm {
class UkmEntryBuilder;
}  // namespace ukm

namespace resource_coordinator {

CoordinationUnitManager::CoordinationUnitManager() {
  CoordinationUnitImpl::AssertNoActiveCoordinationUnits();
}

CoordinationUnitManager::~CoordinationUnitManager() {
  // TODO(oysteine): Keep the map of coordination units as a member of this
  // class, rather than statically inside CoordinationUnitImpl, to avoid this
  // manual lifetime management.
  CoordinationUnitImpl::ClearAllCoordinationUnits();
}

void CoordinationUnitManager::OnStart(
    service_manager::BinderRegistry* registry,
    service_manager::ServiceContextRefFactory* service_ref_factory) {
  provider_ =
      base::MakeUnique<CoordinationUnitProviderImpl>(service_ref_factory, this);

  registry->AddInterface(base::Bind(&CoordinationUnitProviderImpl::Bind,
                                    base::Unretained(provider_.get())));
}

void CoordinationUnitManager::RegisterObserver(
    std::unique_ptr<CoordinationUnitGraphObserver> observer) {
  observers_.push_back(std::move(observer));
}

void CoordinationUnitManager::OnCoordinationUnitCreated(
    CoordinationUnitImpl* coordination_unit) {
  for (auto& observer : observers_) {
    if (observer->ShouldObserve(coordination_unit)) {
      coordination_unit->AddObserver(observer.get());
      observer->OnCoordinationUnitCreated(coordination_unit);
    }
  }
}

void CoordinationUnitManager::OnBeforeCoordinationUnitDestroyed(
    CoordinationUnitImpl* coordination_unit) {
  coordination_unit->BeforeDestroyed();
}

std::unique_ptr<ukm::UkmEntryBuilder>
CoordinationUnitManager::CreateUkmEntryBuilder(const char* event_name) {
  DCHECK(ukm_recorder_ != nullptr);
  return ukm_recorder_->GetEntryBuilder(ukm::UkmRecorder::GetNewSourceID(),
                                        event_name);
}

}  // namespace resource_coordinator
