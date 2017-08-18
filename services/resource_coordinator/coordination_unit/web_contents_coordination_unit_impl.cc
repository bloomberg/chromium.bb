// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"

#include "base/logging.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

namespace resource_coordinator {

WebContentsCoordinationUnitImpl::WebContentsCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {}

WebContentsCoordinationUnitImpl::~WebContentsCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
WebContentsCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) const {
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

void WebContentsCoordinationUnitImpl::RecalculateProperty(
    const mojom::PropertyType property_type) {
  switch (property_type) {
    case mojom::PropertyType::kCPUUsage: {
      double cpu_usage = CalculateCPUUsage();
      SetProperty(mojom::PropertyType::kCPUUsage, cpu_usage);
      break;
    }
    case mojom::PropertyType::kExpectedTaskQueueingDuration: {
      int64_t eqt;
      if (CalculateExpectedTaskQueueingDuration(&eqt))
        SetProperty(property_type, eqt);
      break;
    }
    default:
      NOTREACHED();
  }
}

bool WebContentsCoordinationUnitImpl::IsVisible() const {
  int64_t is_visible;
  bool has_property = GetProperty(mojom::PropertyType::kVisible, &is_visible);
  DCHECK(has_property && (is_visible == 0 || is_visible == 1));
  return is_visible;
}

void WebContentsCoordinationUnitImpl::OnEventReceived(
    const mojom::Event event) {
  for (auto& observer : observers())
    observer.OnWebContentsEventReceived(this, event);
}

void WebContentsCoordinationUnitImpl::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers()) {
    observer.OnWebContentsPropertyChanged(this, property_type, value);
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

    int64_t process_cpu_usage;
    if (process_coordination_unit->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &process_cpu_usage)) {
      cpu_usage += (double)process_cpu_usage / tabs_in_process;
    }
  }

  return cpu_usage;
}

bool WebContentsCoordinationUnitImpl::CalculateExpectedTaskQueueingDuration(
    int64_t* output) {
  // Calculate the EQT for the process of the main frame only because
  // the smoothness of the main frame may affect the users the most.
  CoordinationUnitImpl* main_frame_cu = GetMainFrameCoordinationUnit();
  if (!main_frame_cu)
    return false;

  auto associated_processes =
      main_frame_cu->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);

  size_t num_processes_per_frame = associated_processes.size();
  // A frame should not belong to more than 1 process.
  DCHECK_LE(num_processes_per_frame, 1u);

  if (num_processes_per_frame == 0)
    return false;

  return (*associated_processes.begin())
      ->GetProperty(mojom::PropertyType::kExpectedTaskQueueingDuration, output);
}

CoordinationUnitImpl*
WebContentsCoordinationUnitImpl::GetMainFrameCoordinationUnit() {
  for (auto* cu :
       GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
    if (ToFrameCoordinationUnit(cu)->IsMainFrame())
      return cu;
  }

  return nullptr;
}

}  // namespace resource_coordinator
