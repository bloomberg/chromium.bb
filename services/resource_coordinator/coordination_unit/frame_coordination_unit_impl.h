// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_

#include <set>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"

namespace resource_coordinator {

class FrameCoordinationUnitImpl : public CoordinationUnitImpl {
 public:
  FrameCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~FrameCoordinationUnitImpl() override;

  // CoordinationUnitImpl implementation.
  std::set<CoordinationUnitImpl*> GetAssociatedCoordinationUnitsOfType(
      CoordinationUnitType type) override;

  bool IsMainFrame() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
