// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PROCESS_RESOURCE_COORDINATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PROCESS_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/performance_manager/frame_resource_coordinator.h"
#include "chrome/browser/performance_manager/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace performance_manager {

class ProcessResourceCoordinator
    : public ResourceCoordinatorInterface<
          resource_coordinator::mojom::ProcessCoordinationUnitPtr,
          resource_coordinator::mojom::ProcessCoordinationUnitRequest> {
 public:
  explicit ProcessResourceCoordinator(PerformanceManager* performance_manager);
  ~ProcessResourceCoordinator() override;

  // Must be called immediately after the process is launched.
  void OnProcessLaunched(const base::Process& process);

  void SetCPUUsage(double usage);
  void SetProcessExitStatus(int32_t exit_status);

 private:
  void ConnectToService(
      resource_coordinator::mojom::CoordinationUnitProviderPtr& provider,
      const resource_coordinator::CoordinationUnitID& cu_id) override;

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<ProcessResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProcessResourceCoordinator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PROCESS_RESOURCE_COORDINATOR_H_
