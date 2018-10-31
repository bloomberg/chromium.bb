// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"

#include "base/time/time.h"
#include "build/build_config.h"

namespace resource_coordinator {

ProcessResourceCoordinator::ProcessResourceCoordinator(
    service_manager::Connector* connector)
    : ResourceCoordinatorInterface(), weak_ptr_factory_(this) {
  CoordinationUnitID new_cu_id(CoordinationUnitType::kProcess,
                               CoordinationUnitID::RANDOM_ID);
  ResourceCoordinatorInterface::ConnectToService(connector, new_cu_id);
  DCHECK(service_);
}

ProcessResourceCoordinator::~ProcessResourceCoordinator() = default;

void ProcessResourceCoordinator::OnProcessLaunched(
    const base::Process& process) {
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
  service_->SetCPUUsage(cpu_usage);
}

void ProcessResourceCoordinator::AddFrame(
    const FrameResourceCoordinator& frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  frame.service()->GetID(
      base::BindOnce(&ProcessResourceCoordinator::AddFrameByID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProcessResourceCoordinator::ConnectToService(
    mojom::CoordinationUnitProviderPtr& provider,
    const CoordinationUnitID& cu_id) {
  provider->CreateProcessCoordinationUnit(mojo::MakeRequest(&service_), cu_id);
}

void ProcessResourceCoordinator::AddFrameByID(const CoordinationUnitID& cu_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  service_->AddFrame(cu_id);
}

}  // namespace resource_coordinator
