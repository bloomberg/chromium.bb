// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"

namespace service_manager {
class ServiceContextRef;
}

namespace resource_coordinator {

struct CoordinationUnitID;

class WebContentsCoordinationUnitImpl : public CoordinationUnitImpl {
 public:
  WebContentsCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~WebContentsCoordinationUnitImpl() override;

  // CoordinationUnitImpl implementation.
  std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) override;
  void RecalculateProperty(mojom::PropertyType property_type) override;

 private:
  double CalculateCPUUsage();

  DISALLOW_COPY_AND_ASSIGN(WebContentsCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_WEB_CONTENTS_COORDINATION_UNIT_IMPL_H_
