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

  base::TimeDelta time_since_last_measurement;
  if (!last_measurement_batch_time_.is_null()) {
    // Use the end of the measurement batch as a proxy for when every
    // measurement was acquired. For the purpose of estimating CPU usage
    // over the duration from last measurement, it'll be near enough. The error
    // will average out, and there's an inherent race in knowing when a
    // measurement was actually acquired in any case.
    time_since_last_measurement =
        measurement_batch->batch_ended_time - last_measurement_batch_time_;
    DCHECK_LE(base::TimeDelta(), time_since_last_measurement);
  }
  last_measurement_batch_time_ = measurement_batch->batch_ended_time;

  for (const auto& measurement : measurement_batch->measurements) {
    for (auto it = processes.begin(); it != processes.end(); ++it) {
      ProcessCoordinationUnitImpl* process = *it;
      int64_t process_pid;
      // TODO(siggi): This seems pretty silly - we're going O(N^2) in processes
      //     here, and going through a relatively expensive accessor for the
      //     PID.
      if (process->GetProperty(mojom::PropertyType::kPID, &process_pid) &&
          static_cast<base::ProcessId>(process_pid) == measurement->pid) {
        if (process->cumulative_cpu_usage().is_zero() ||
            time_since_last_measurement.is_zero()) {
          // Imitate the behavior of GetPlatformIndependentCPUUsage, which
          // yields zero for the initial measurement of each process.
          process->SetCPUUsage(0.0);
        } else {
          base::TimeDelta cumulative_cpu_delta =
              measurement->cpu_usage - process->cumulative_cpu_usage();

          double cpu_usage =
              static_cast<double>(cumulative_cpu_delta.InMicroseconds() * 100) /
              time_since_last_measurement.InMicroseconds();
          process->SetCPUUsage(cpu_usage);
        }
        process->set_cumulative_cpu_usage(measurement->cpu_usage);
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
