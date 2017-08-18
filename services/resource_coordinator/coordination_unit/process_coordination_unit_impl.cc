// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include "base/logging.h"
#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"

namespace resource_coordinator {

ProcessCoordinationUnitImpl::ProcessCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {}

ProcessCoordinationUnitImpl::~ProcessCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
ProcessCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  switch (type) {
    case CoordinationUnitType::kWebContents: {
      // There is currently not a direct relationship between processes and
      // tabs. However, frames are children of both processes and frames, so we
      // find all of the tabs that are reachable from the process's child
      // frames.
      std::set<CoordinationUnitImpl*> web_contents_coordination_units;

      for (auto* frame_coordination_unit :
           GetChildCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
        for (auto* web_contents_coordination_unit :
             frame_coordination_unit->GetAssociatedCoordinationUnitsOfType(
                 CoordinationUnitType::kWebContents)) {
          web_contents_coordination_units.insert(
              web_contents_coordination_unit);
        }
      }

      return web_contents_coordination_units;
    }
    case CoordinationUnitType::kFrame:
      return GetChildCoordinationUnitsOfType(type);
    default:
      return std::set<CoordinationUnitImpl*>();
  }
}

void ProcessCoordinationUnitImpl::PropagateProperty(
    mojom::PropertyType property_type,
    int64_t value) {
  switch (property_type) {
    // Trigger WebContents CU to recalculate their CPU usage.
    case mojom::PropertyType::kCPUUsage: {
      for (auto* web_contents_cu : GetAssociatedCoordinationUnitsOfType(
               CoordinationUnitType::kWebContents)) {
        web_contents_cu->RecalculateProperty(mojom::PropertyType::kCPUUsage);
      }
      break;
    }
    case mojom::PropertyType::kExpectedTaskQueueingDuration: {
      // Do not propagate if the associated frame is not the main frame.
      for (auto* cu :
           GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
        auto* frame_cu = ToFrameCoordinationUnit(cu);
        if (!frame_cu->IsMainFrame())
          continue;

        auto* web_contents_cu = frame_cu->GetWebContentsCoordinationUnit();

        if (web_contents_cu) {
          web_contents_cu->RecalculateProperty(
              mojom::PropertyType::kExpectedTaskQueueingDuration);
        }
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace resource_coordinator
