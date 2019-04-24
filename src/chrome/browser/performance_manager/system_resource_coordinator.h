// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_

#include "chrome/browser/performance_manager/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class SystemResourceCoordinator
    : public ResourceCoordinatorInterface<
          resource_coordinator::mojom::SystemCoordinationUnitPtr,
          resource_coordinator::mojom::SystemCoordinationUnitRequest> {
 public:
  explicit SystemResourceCoordinator(PerformanceManager* performance_manager);
  ~SystemResourceCoordinator() override;

  void DistributeMeasurementBatch(
      resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch);

 private:
  void ConnectToService(
      resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
      const resource_coordinator::CoordinationUnitID& cu_id) override;

  DISALLOW_COPY_AND_ASSIGN(SystemResourceCoordinator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_SYSTEM_RESOURCE_COORDINATOR_H_
