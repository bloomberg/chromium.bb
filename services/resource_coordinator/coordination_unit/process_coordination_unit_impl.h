// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

namespace resource_coordinator {

class ProcessCoordinationUnitImpl : public CoordinationUnitImpl {
 public:
  ProcessCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~ProcessCoordinationUnitImpl() override;

  // CoordinationUnitImpl implementation.
  std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) override;

 private:
  // CoordinationUnitImpl implementation.
  void PropagateProperty(mojom::PropertyType property_type,
                         const base::Value& value) override;

  DISALLOW_COPY_AND_ASSIGN(ProcessCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PROCESS_COORDINATION_UNIT_IMPL_H_
