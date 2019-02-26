// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_

#include <map>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/timer/timer.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/system_resource_coordinator.h"

namespace resource_coordinator {

// |RenderProcessProbe| collects measurements about render
// processes and propagates them to the |resource_coordinator| service.
// Currently this is only supported for Chrome Metrics experiments.
// The measurements are initiated from the UI thread, while acquiring and
// dispatching the measurements is done on the IO thread.
// This interface is broken out for testing.
class RenderProcessProbe {
 public:
  // Returns the current |RenderProcessProbe| instance
  // if one exists; otherwise it constructs a new instance.
  static RenderProcessProbe* GetInstance();

  static bool IsEnabled();

  virtual ~RenderProcessProbe() = default;

  // Starts a single collection cycle if a cycle is not already in progress.
  // If a cycle is already in progress, this method will simply return.
  virtual void StartSingleGather() = 0;
};

class RenderProcessProbeImpl : public RenderProcessProbe {
 public:
  void StartSingleGather() override;

 protected:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MeasurementOutcome {
    kMeasurementSuccess = 0,
    kMeasurementPartialSuccess = 1,
    kMeasurementFailure = 2,

    kMaxValue = kMeasurementFailure,
  };

  static constexpr base::TimeDelta kUninitializedCPUTime =
      base::TimeDelta::FromMicroseconds(-1);

  // Internal state protected for testing.
  struct RenderProcessInfo {
    RenderProcessInfo();
    ~RenderProcessInfo();
    base::Process process;
    base::TimeDelta cpu_usage = kUninitializedCPUTime;
    size_t last_gather_cycle_active = -1;
    std::unique_ptr<base::ProcessMetrics> metrics;
  };
  using RenderProcessInfoMap = std::map<int, RenderProcessInfo>;

  friend class base::NoDestructor<RenderProcessProbeImpl>;

  RenderProcessProbeImpl();
  ~RenderProcessProbeImpl() override;

  // (1) Identify all of the render processes that are active to measure.
  // Child render processes can only be discovered in the browser's UI thread.
  void RegisterAliveRenderProcessesOnUIThread();
  // (2) Collect the render process CPU metrics and initiate a memory dump.
  void CollectRenderProcessMetricsAndStartMemoryDumpOnIOThread();
  // (3) Process the results of the memory dump and dispatch the results.
  void ProcessGlobalMemoryDumpAndDispatchOnIOThread(
      base::TimeTicks collection_start_time,
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);
  // (4) Finish the collection cycle on the UI thread.
  void FinishCollectionOnUIThread(
      mojom::ProcessResourceMeasurementBatchPtr batch);

  // Test seams.
  virtual void RegisterRenderProcesses();
  virtual void StartMemoryMeasurement(base::TimeTicks collection_start_time);
  virtual base::ProcessId GetProcessId(int host_id,
                                       const RenderProcessInfo& info);

  SystemResourceCoordinator* EnsureSystemResourceCoordinator();

  // Dispatch the collected metrics.
  // Virtual for testing.
  virtual void DispatchMetricsOnUIThread(
      mojom::ProcessResourceMeasurementBatchPtr batch);

  // A map of currently running render process host IDs to process.
  // This map is accessed alternatively from the UI thread and the IO thread,
  // but only one of the two at a time.
  RenderProcessInfoMap render_process_info_map_;

  // Number of measurements collected so far.
  size_t current_gather_cycle_ = 0u;

  // True while a gathering cycle is underways on a background thread.
  bool is_gathering_ = false;

  // Used to signal the end of a CPU measurement cycle to the RC.
  std::unique_ptr<SystemResourceCoordinator> system_resource_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessProbeImpl);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RENDER_PROCESS_PROBE_H_
