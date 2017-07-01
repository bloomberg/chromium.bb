// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"

#include <utility>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "base/values.h"

#if defined(OS_MACOSX)
#include "services/service_manager/public/cpp/standalone_service/mach_broker.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace service_manager {
class ServiceContextRef;
}

namespace resource_coordinator {

struct CoordinationUnitID;

namespace {

const int kCPUProfilingIntervalInSeconds = 5;

}  // namespace

ProcessCoordinationUnitImpl::ProcessCoordinationUnitImpl(
    const CoordinationUnitID& id,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : CoordinationUnitImpl(id, std::move(service_ref)) {
  // ProcessCoordinationUnit ids should correspond to its pid
  base::ProcessId pid = id.id;
#if defined(OS_WIN)
  base::Process process =
      base::Process::OpenWithAccess(pid, PROCESS_QUERY_INFORMATION);
#else
  base::Process process = base::Process::Open(pid);
#endif
  base::ProcessHandle process_handle = process.Handle();

#if defined(OS_MACOSX)
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(
      process_handle,
      service_manager::MachBroker::GetInstance()->port_provider());
#else
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(process_handle);
#endif

  repeating_timer_.Start(FROM_HERE, base::TimeDelta(), this,
                         &ProcessCoordinationUnitImpl::MeasureProcessCPUUsage);
}

ProcessCoordinationUnitImpl::~ProcessCoordinationUnitImpl() = default;

std::set<CoordinationUnitImpl*>
ProcessCoordinationUnitImpl::GetAssociatedCoordinationUnitsOfType(
    CoordinationUnitType type) {
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

void ProcessCoordinationUnitImpl::MeasureProcessCPUUsage() {
  double cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage();
  SetProperty(mojom::PropertyType::kCPUUsage, base::Value(cpu_usage));

  // Trigger tab coordination units to recalculate their CPU usage.
  // TODO(matthalp): Move propagation functionality into a separate,
  // more generalized method to support other property changes.
  for (auto* tab : GetAssociatedCoordinationUnitsOfType(
           CoordinationUnitType::kWebContents)) {
    tab->RecalculateProperty(mojom::PropertyType::kCPUUsage);
  }

  repeating_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kCPUProfilingIntervalInSeconds),
      this, &ProcessCoordinationUnitImpl::MeasureProcessCPUUsage);
}

}  // namespace resource_coordinator
