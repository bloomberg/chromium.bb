// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

namespace resource_coordinator {

class ProcessCoordinationUnitImpl : public CoordinationUnitBase,
                                    public mojom::ProcessCoordinationUnit {
 public:
  ProcessCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~ProcessCoordinationUnitImpl() override;

  // CoordinationUnitBase implementation.
  // mojom::ProcessCoordinationUnit implementation.
  void SetCPUUsage(double cpu_usage) override;
  void SetExpectedTaskQueueingDuration(base::TimeDelta duration) override;
  void SetLaunchTime(base::Time launch_time) override;
  void SetPID(int64_t pid) override;

  std::set<CoordinationUnitBase*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) const override;

 private:
  // CoordinationUnitBase implementation.
  void PropagateProperty(mojom::PropertyType property_type,
                         int64_t value) override;

  DISALLOW_COPY_AND_ASSIGN(ProcessCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
