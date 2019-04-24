// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/process_resource_coordinator.h"

#include "base/time/time.h"
#include "build/build_config.h"

namespace performance_manager {

ProcessResourceCoordinator::ProcessResourceCoordinator(
    PerformanceManager* performance_manager)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  resource_coordinator::CoordinationUnitID new_cu_id(
      resource_coordinator::CoordinationUnitType::kProcess,
      resource_coordinator::CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(performance_manager,
                                                 new_cu_id);
}

ProcessResourceCoordinator::~ProcessResourceCoordinator() = default;

void ProcessResourceCoordinator::OnProcessLaunched(
    const base::Process& process) {
  if (!service_)
    return;

  // TODO(fdoray): Merge ProcessCoordinationUnit::SetPID/SetLaunchTime().
  service_->SetPID(process.Pid());
  service_->SetLaunchTime(
#if defined(OS_ANDROID)
      // Process::CreationTime() is not available on Android. Since this method
      // is called immediately after the process is launched, the process launch
      // time can be approximated with the current time.
      base::Time::Now()
#else
      process.CreationTime()
#endif
  );
}

void ProcessResourceCoordinator::SetCPUUsage(double cpu_usage) {
  if (!service_)
    return;

  service_->SetCPUUsage(cpu_usage);
}

void ProcessResourceCoordinator::SetProcessExitStatus(int32_t exit_status) {
  if (!service_)
    return;

  service_->SetProcessExitStatus(exit_status);
}

void ProcessResourceCoordinator::ConnectToService(
    resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
    const resource_coordinator::CoordinationUnitID& cu_id) {
  provider->CreateProcessCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

}  // namespace performance_manager
