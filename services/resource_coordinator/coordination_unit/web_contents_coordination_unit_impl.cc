// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"

namespace resource_coordinator {

WebContentsCoordinationUnitImpl::WebContentsCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {}

WebContentsCoordinationUnitImpl::~WebContentsCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
WebContentsCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) {
  switch (type) {
    case CoordinationUnitType::kProcess: {
      std::set<CoordinationUnitImpl*> process_coordination_units;

      // There is currently not a direct relationship between processes and
      // tabs. However, frames are children of both processes and frames, so we
      // find all of the processes that are reachable from the tabs's child
      // frames.
      for (auto* frame_coordination_unit :
           GetChildCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
        for (auto* process_coordination_unit :
             frame_coordination_unit->GetAssociatedCoordinationUnitsOfType(
                 CoordinationUnitType::kProcess)) {
          process_coordination_units.insert(process_coordination_unit);
        }
      }

      return process_coordination_units;
    }
    case CoordinationUnitType::kFrame:
      return GetChildCoordinationUnitsOfType(type);
    default:
      return std::set<CoordinationUnitImpl*>();
  }
}

double WebContentsCoordinationUnitImpl::CalculateCPUUsage() {
  double cpu_usage = 0.0;

  for (auto* process_coordination_unit :
       GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kProcess)) {
    size_t tabs_in_process = process_coordination_unit
                                 ->GetAssociatedCoordinationUnitsOfType(
                                     CoordinationUnitType::kWebContents)
                                 .size();
    DCHECK_LE(1u, tabs_in_process);

    base::Value process_cpu_usage_value =
        process_coordination_unit->GetProperty(mojom::PropertyType::kCPUUsage);
    double process_cpu_usage =
        process_cpu_usage_value.IsType(base::Value::Type::NONE)
            ? 0.0
            : process_cpu_usage_value.GetDouble();
    cpu_usage += process_cpu_usage / tabs_in_process;
  }

  return cpu_usage;
}

void WebContentsCoordinationUnitImpl::RecalculateProperty(
    const mojom::PropertyType property_type) {
  if (property_type == mojom::PropertyType::kCPUUsage) {
    double cpu_usage = CalculateCPUUsage();

    SetProperty(mojom::PropertyType::kCPUUsage,
                base::MakeUnique<base::Value>(cpu_usage));
  }
}

}  // namespace resource_coordinator
