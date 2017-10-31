// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include "base/logging.h"
#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

namespace resource_coordinator {

ProcessCoordinationUnitImpl::ProcessCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitBase(id, std::move(service_ref)) {}

ProcessCoordinationUnitImpl::~ProcessCoordinationUnitImpl() = default;

void ProcessCoordinationUnitImpl::SetCPUUsage(double cpu_usage) {
  SetProperty(mojom::PropertyType::kCPUUsage, cpu_usage * 1000);
}

void ProcessCoordinationUnitImpl::SetExpectedTaskQueueingDuration(
    base::TimeDelta duration) {
  SetProperty(mojom::PropertyType::kExpectedTaskQueueingDuration,
              duration.InMilliseconds());
}

void ProcessCoordinationUnitImpl::SetLaunchTime(base::Time launch_time) {
  SetProperty(mojom::PropertyType::kLaunchTime, launch_time.ToTimeT());
}

void ProcessCoordinationUnitImpl::SetPID(int64_t pid) {
  SetProperty(mojom::PropertyType::kPID, pid);
}

std::set<CoordinationUnitBase*>
ProcessCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  switch (type) {
    case CoordinationUnitType::kPage: {
      // There is currently not a direct relationship between processes and
      // pages. However, frames are children of both processes and frames, so we
      // find all of the pages that are reachable from the process's child
      // frames.
      std::set<CoordinationUnitBase*> page_cus;

      for (auto* frame_cu :
           GetChildCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
        for (auto* page_cu : frame_cu->GetAssociatedCoordinationUnitsOfType(
                 CoordinationUnitType::kPage)) {
          page_cus.insert(page_cu);
        }
      }

      return page_cus;
    }
    case CoordinationUnitType::kFrame:
      return GetChildCoordinationUnitsOfType(type);
    default:
      return std::set<CoordinationUnitBase*>();
  }
}

void ProcessCoordinationUnitImpl::PropagateProperty(
    mojom::PropertyType property_type,
    int64_t value) {
  switch (property_type) {
    // Trigger Page CU to recalculate their CPU usage.
    case mojom::PropertyType::kCPUUsage: {
      for (auto* page_cu :
           GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kPage)) {
        page_cu->RecalculateProperty(mojom::PropertyType::kCPUUsage);
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

        auto* page_cu = frame_cu->GetPageCoordinationUnit();

        if (page_cu) {
          page_cu->RecalculateProperty(
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
