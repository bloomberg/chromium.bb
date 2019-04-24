// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/system_resource_coordinator.h"

namespace performance_manager {

SystemResourceCoordinator::SystemResourceCoordinator(
    PerformanceManager* performance_manager)
    : ResourceCoordinatorInterface() {
  resource_coordinator::CoordinationUnitID new_cu_id(
      resource_coordinator::CoordinationUnitType::kSystem,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(performance_manager,
                                                 new_cu_id);
}

SystemResourceCoordinator::~SystemResourceCoordinator() = default;

void SystemResourceCoordinator::DistributeMeasurementBatch(
    resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch) {
  if (!service_)
    return;
  service_->DistributeMeasurementBatch(std::move(batch));
}

void SystemResourceCoordinator::ConnectToService(
    resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
    const resource_coordinator::CoordinationUnitID& cu_id) {
  provider->GetSystemCoordinationUnit(mojo::MakeRequest(&service_));
}

}  // namespace performance_manager
