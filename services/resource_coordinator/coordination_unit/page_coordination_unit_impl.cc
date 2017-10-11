// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

#include "base/logging.h"
#include "base/time/default_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"

namespace resource_coordinator {

PageCoordinationUnitImpl::PageCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitBase(id, std::move(service_ref)),
      clock_(new base::DefaultTickClock()) {}

PageCoordinationUnitImpl::~PageCoordinationUnitImpl() = default;

std::set<CoordinationUnitBase*>
PageCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) const {
  switch (type) {
    case CoordinationUnitType::kProcess: {
      std::set<CoordinationUnitBase*> process_coordination_units;

      // There is currently not a direct relationship between processes and
      // pages. However, frames are children of both processes and frames, so we
      // find all of the processes that are reachable from the pages's child
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
      return std::set<CoordinationUnitBase*>();
  }
}

void PageCoordinationUnitImpl::RecalculateProperty(
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

bool PageCoordinationUnitImpl::IsVisible() const {
  int64_t is_visible;
  bool has_property = GetProperty(mojom::PropertyType::kVisible, &is_visible);
  DCHECK(has_property && (is_visible == 0 || is_visible == 1));
  return is_visible;
}

base::TimeDelta PageCoordinationUnitImpl::TimeSinceLastNavigation() const {
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return clock_->NowTicks() - navigation_committed_time_;
}

base::TimeDelta PageCoordinationUnitImpl::TimeSinceLastVisibilityChange()
    const {
  return clock_->NowTicks() - visibility_change_time_;
}

void PageCoordinationUnitImpl::SetClockForTest(
    std::unique_ptr<base::TickClock> test_clock) {
  clock_ = std::move(test_clock);
}

void PageCoordinationUnitImpl::OnEventReceived(const mojom::Event event) {
  if (event == mojom::Event::kNavigationCommitted)
    navigation_committed_time_ = clock_->NowTicks();
  for (auto& observer : observers())
    observer.OnPageEventReceived(this, event);
}

void PageCoordinationUnitImpl::OnPropertyChanged(
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kVisible)
    visibility_change_time_ = clock_->NowTicks();
  for (auto& observer : observers()) {
    observer.OnPagePropertyChanged(this, property_type, value);
  }
}

double PageCoordinationUnitImpl::CalculateCPUUsage() {
  double cpu_usage = 0.0;

  for (auto* process_coordination_unit :
       GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kProcess)) {
    size_t pages_in_process =
        process_coordination_unit
            ->GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kPage)
            .size();
    DCHECK_LE(1u, pages_in_process);

    int64_t process_cpu_usage;
    if (process_coordination_unit->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &process_cpu_usage))
      cpu_usage += (double)process_cpu_usage / pages_in_process;
  }

  return cpu_usage;
}

bool PageCoordinationUnitImpl::CalculateExpectedTaskQueueingDuration(
    int64_t* output) {
  // Calculate the EQT for the process of the main frame only because
  // the smoothness of the main frame may affect the users the most.
  CoordinationUnitBase* main_frame_cu = GetMainFrameCoordinationUnit();
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

CoordinationUnitBase* PageCoordinationUnitImpl::GetMainFrameCoordinationUnit() {
  for (auto* cu :
       GetAssociatedCoordinationUnitsOfType(CoordinationUnitType::kFrame)) {
    if (ToFrameCoordinationUnit(cu)->IsMainFrame())
      return cu;
  }

  return nullptr;
}

}  // namespace resource_coordinator
