// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include "base/macros.h"
#include "base/process/process_handle.h"

namespace resource_coordinator {

SystemCoordinationUnitImpl::SystemCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitInterface(id, std::move(service_ref)) {}

SystemCoordinationUnitImpl::~SystemCoordinationUnitImpl() = default;

void SystemCoordinationUnitImpl::OnProcessCPUUsageReady() {
  SendEvent(mojom::Event::kProcessCPUUsageReady);
}

void SystemCoordinationUnitImpl::DistributeMeasurementBatch(
    mojom::ProcessResourceMeasurementBatchPtr measurement_batch) {
  // Grab all the processes.
  std::vector<ProcessCoordinationUnitImpl*> processes =
      ProcessCoordinationUnitImpl::GetAllProcessCoordinationUnits();

  for (const auto& measurement : measurement_batch->measurements) {
    for (auto it = processes.begin(); it != processes.end(); ++it) {
      ProcessCoordinationUnitImpl* process = *it;
      int64_t process_pid;
      // TODO(siggi): This seems pretty silly - we're going O(N^2) in processes
      //     here, and going through a relatively expensive accessor for the
      //     PID.
      if (process->GetProperty(mojom::PropertyType::kPID, &process_pid) &&
          static_cast<base::ProcessId>(process_pid) == measurement->pid) {
        process->SetCPUUsage(measurement->cpu_usage);
        process->set_private_footprint_kb(measurement->private_footprint_kb);

        // Remove found processes.
        processes.erase(it);
        break;
      }
    }
  }

  // Clear processes we didn't get data for.
  for (ProcessCoordinationUnitImpl* process : processes) {
    process->SetCPUUsage(0.0);
    process->set_private_footprint_kb(0);
  }

  // Fire the end update signal.
  OnProcessCPUUsageReady();
}

void SystemCoordinationUnitImpl::OnEventReceived(mojom::Event event) {
  for (auto& observer : observers())
    observer.OnSystemEventReceived(this, event);
}

void SystemCoordinationUnitImpl::OnPropertyChanged(
    mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers())
    observer.OnSystemPropertyChanged(this, property_type, value);
}

}  // namespace resource_coordinator
