// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_chromeos.h"

#include "ash/components/arc/arc_util.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/process/arc_process.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer.h"
#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

namespace {
// TODO(crbug.com/1189677): Remove the global static variable and make it
// GraphOwned once performance_manager code is migrated to UI thread.
WorkingSetTrimmerPolicyChromeOS::ArcVmDelegate* g_arcvm_delegate_for_testing =
    nullptr;

// Reports ARCVM trim metrics every |kArcVmTrimMetricReportDelay| minutes.
constexpr base::TimeDelta kArcVmTrimMetricReportDelay = base::Minutes(30);

// It is very unlikely to do the trim more than |kArcVmTrimMetricMaxCount|
// times in |kArcVmTrimMetricReportDelay|.
constexpr int kArcVmTrimMetricMaxCount = 30;

enum ArcProcessType { kApp, kSystem };
void GetArcProcessListOnUIThread(
    ArcProcessType type,
    base::WeakPtr<
        performance_manager::policies::WorkingSetTrimmerPolicyChromeOS> ptr,
    int processes_per_trim) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service) {
    return;
  }

  // Now we need to bounce back to the PM sequence so we can do stuff with the
  // process list.
  auto callback = base::BindOnce(
      [](decltype(ptr) ptr, decltype(processes_per_trim) processes_per_trim,
         arc::ArcProcessService::OptionalArcProcessList opt_proc_list) {
        PerformanceManager::CallOnGraph(
            FROM_HERE,
            base::BindOnce(
                &WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses, ptr,
                processes_per_trim, std::move(opt_proc_list)));
      },
      ptr, processes_per_trim);

  if (type == kApp) {
    arc_process_service->RequestAppProcessList(std::move(callback));
  } else if (type == kSystem) {
    arc_process_service->RequestSystemProcessList(std::move(callback));
  }
}

void OnTrimArcVmWorkingSetOnUIThread(bool success,
                                     const std::string& failure_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (success) {
    VLOG(2) << "Reclaimed ARCVM memory";
    return;
  }
  LOG(WARNING) << "Failed to reclaim ARCVM memory: " << failure_reason;
}

}  // namespace

WorkingSetTrimmerPolicyChromeOS::WorkingSetTrimmerPolicyChromeOS() {
  trim_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimOnMemoryPressure);
  trim_on_freeze_ = base::FeatureList::IsEnabled(features::kTrimOnFreeze);
  trim_arc_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimArcOnMemoryPressure);
  trim_arcvm_on_memory_pressure_ =
      base::FeatureList::IsEnabled(features::kTrimArcVmOnMemoryPressure);

  params_ = features::TrimOnMemoryPressureParams::GetParams();

  if (trim_arc_on_memory_pressure_) {
    // Validate ARC parameters.
    if (!params_.trim_arc_app_processes && !params_.trim_arc_system_processes) {
      LOG(ERROR)
          << "Misconfiguration ARC trimming on memory pressure is enabled "
             "but both app and system process trimming are disabled.";
      trim_arc_on_memory_pressure_ = false;
    } else if (!arc::IsArcAvailable()) {
      DLOG(ERROR) << "ARC is not available";
      trim_arc_on_memory_pressure_ = false;
    } else if (arc::IsArcVmEnabled()) {
      // ARCVM is handled separately.
      trim_arc_on_memory_pressure_ = false;
    }
  }

  if (trim_arcvm_on_memory_pressure_) {
    if (!arc::IsArcAvailable() || !arc::IsArcVmEnabled()) {
      DLOG(ERROR) << "ARCVM is not available";
      trim_arcvm_on_memory_pressure_ = false;
    }
  }
}

WorkingSetTrimmerPolicyChromeOS::~WorkingSetTrimmerPolicyChromeOS() = default;

// On MemoryPressure we will try to trim the working set of some renders if they
// have been backgrounded for some period of time and have not been trimmed for
// at least the backoff period.
void WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Try not to walk the graph too frequently because we can receive moderate
  // memory pressure notifications every 10s.

  if (trim_on_memory_pressure_) {
    if (!last_graph_walk_ || (base::TimeTicks::Now() - *last_graph_walk_ >
                              params_.graph_walk_backoff_time)) {
      TrimNodesOnGraph();
    }
  }

  if (trim_arc_on_memory_pressure_) {
    if (!last_arc_process_fetch_ ||
        (base::TimeTicks::Now() - *last_arc_process_fetch_ >
         params_.arc_process_list_fetch_backoff_time)) {
      TrimArcProcesses();
    }
  }

  if (trim_arcvm_on_memory_pressure_) {
    if (!last_arcvm_trim_ || (base::TimeTicks::Now() - *last_arcvm_trim_ >
                              params_.arcvm_trim_backoff_time)) {
      TrimArcVmProcesses(level);
    }
  }
}

void WorkingSetTrimmerPolicyChromeOS::set_arcvm_delegate_for_testing(
    ArcVmDelegate* delegate) {
  DCHECK(!g_arcvm_delegate_for_testing || !delegate);
  g_arcvm_delegate_for_testing = delegate;
}

void WorkingSetTrimmerPolicyChromeOS::TrimNodesOnGraph() {
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  for (const PageNode* page_node : graph_->GetAllPageNodes()) {
    if (!page_node->IsVisible() &&
        page_node->GetTimeSinceLastVisibilityChange() >
            params_.node_invisible_time) {
      // Get the process node and if it has not been
      // trimmed within the backoff period, we will do that
      // now.

      // Check that we have a main frame.
      const FrameNode* frame_node = page_node->GetMainFrameNode();
      if (!frame_node) {
        continue;
      }

      const ProcessNode* process_node = frame_node->GetProcessNode();
      if (process_node && process_node->GetProcess().IsValid()) {
        base::TimeTicks last_trim = GetLastTrimTime(process_node);
        if (now_ticks - last_trim > params_.node_trim_backoff_time) {
          TrimWorkingSet(process_node);
        }
      }
    }
  }
  last_graph_walk_ = now_ticks;
}

base::TimeDelta WorkingSetTrimmerPolicyChromeOS::GetTimeSinceLastArcProcessTrim(
    base::ProcessId pid) const {
  base::TimeDelta delta(base::TimeDelta::Max());
  const auto it = arc_processes_last_trim_.find(pid);
  if (it != arc_processes_last_trim_.end()) {
    delta = base::TimeTicks::Now() - it->second;
  }
  return delta;
}

void WorkingSetTrimmerPolicyChromeOS::SetArcProcessLastTrimTime(
    base::ProcessId pid,
    base::TimeTicks time) {
  arc_processes_last_trim_[pid] = time;
}

bool WorkingSetTrimmerPolicyChromeOS::IsArcProcessEligibleForReclaim(
    const arc::ArcProcess& arc_process) {
  // Focused apps will never be reclaimed.
  if (arc_process.is_focused()) {
    return false;
  }

  if (!params_.trim_arc_aggressive) {
    // By default (non-aggressive) we will only trim unimportant apps
    // non-background protected apps.
    if (arc_process.IsImportant() || arc_process.IsBackgroundProtected()) {
      return false;
    }
  }

  // Next we need to check if it's been reclaimed too recently, if configured.
  if (params_.arc_process_trim_backoff_time != base::TimeDelta::Min()) {
    if (GetTimeSinceLastArcProcessTrim(arc_process.pid()) <
        params_.arc_process_trim_backoff_time) {
      return false;
    }
  }

  // Finally we check if the last activity time was longer than the configured
  // threshold, if configured.
  if (params_.arc_process_inactivity_time != base::TimeDelta::Min()) {
    // Are we within the threshold?
    if ((base::TimeTicks::Now() -
         base::TimeTicks::FromUptimeMillis(arc_process.last_activity_time())) <
        params_.arc_process_inactivity_time) {
      return false;
    }
  }

  return true;
}

bool WorkingSetTrimmerPolicyChromeOS::TrimArcProcess(base::ProcessId pid) {
  SetArcProcessLastTrimTime(pid, base::TimeTicks::Now());

  static int arc_processes_trimmed = 0;
  base::UmaHistogramCounts10000("Memory.WorkingSetTrim.ArcProcessTrimCount",
                                ++arc_processes_trimmed);

  auto* trimmer = static_cast<mechanism::WorkingSetTrimmerChromeOS*>(
      mechanism::WorkingSetTrimmer::GetInstance());
  return trimmer->TrimWorkingSet(pid);
}

void WorkingSetTrimmerPolicyChromeOS::TrimReceivedArcProcesses(
    int allowed_to_trim,
    arc::ArcProcessService::OptionalArcProcessList arc_processes) {
  if (!arc_processes.has_value()) {
    return;
  }

  // Because ARC may return the same list in order, we shuffle them each time in
  // case we have a small number of arc_available_processes_to_trim_ we don't
  // want to retrim the same ones every time.
  auto& procs = arc_processes.value();
  base::RandomShuffle(procs.begin(), procs.end());

  for (const auto& proc : procs) {
    // If we've already reclaimed too much, we bail.
    if (!allowed_to_trim) {
      break;
    }

    if (IsArcProcessEligibleForReclaim(proc)) {
      TrimArcProcess(proc.pid());

      if (allowed_to_trim > 0) {
        allowed_to_trim--;
      }
    }
  }
}

// TrimArcProcesses will be called on the PM Sequence, we'll need to bounce to
// the UI thread to get the Arc process list and we'll bounce back to the PM
// sequence to do the actual trimming and book keeping.
void WorkingSetTrimmerPolicyChromeOS::TrimArcProcesses() {
  last_arc_process_fetch_ = base::TimeTicks::Now();

  // The fetching of the ARC process list must happen on the UI thread.
  if (params_.trim_arc_system_processes) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetArcProcessListOnUIThread, ArcProcessType::kSystem,
                       weak_ptr_factory_.GetWeakPtr(),
                       params_.arc_max_number_processes_per_trim));
  }

  if (params_.trim_arc_app_processes) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetArcProcessListOnUIThread, ArcProcessType::kApp,
                       weak_ptr_factory_.GetWeakPtr(),
                       params_.arc_max_number_processes_per_trim));
  }
}

void WorkingSetTrimmerPolicyChromeOS::TrimArcVmProcesses(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK_NE(level, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  // TODO(crbug.com/1189677): Remove the PostTask once performance_manager code
  // is migrated to UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TrimArcVmProcessesOnUIThread, level, params_,
                                weak_ptr_factory_.GetWeakPtr()));
}

// static
void WorkingSetTrimmerPolicyChromeOS::TrimArcVmProcessesOnUIThread(
    base::MemoryPressureListener::MemoryPressureLevel level,
    features::TrimOnMemoryPressureParams params,
    base::WeakPtr<WorkingSetTrimmerPolicyChromeOS> ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/1189677): Let the policy own WorkingSetTrimmerPolicyArcVm
  // instance once performance_manager code is migrated to UI thread.
  auto* arcvm_delegate = g_arcvm_delegate_for_testing
                             ? g_arcvm_delegate_for_testing
                             : WorkingSetTrimmerPolicyArcVm::Get();

  const bool force_reclaim =
      params.trim_arcvm_on_critical_pressure &&
      (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  const bool need_reclaim =
      force_reclaim ||
      arcvm_delegate->IsEligibleForReclaim(
          params.arcvm_inactivity_time,
          params.trim_arcvm_on_first_memory_pressure_after_arcvm_boot);
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmProcesses,
                     ptr, need_reclaim));
}

void WorkingSetTrimmerPolicyChromeOS::OnTrimArcVmProcesses(bool need_reclaim) {
  if (!need_reclaim)
    return;
  // TODO(crbug.com/1189677): Remove the PostTask once performance_manager code
  // is migrated to UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindRepeating(&DoTrimArcVmOnUIThread));
  last_arcvm_trim_ = base::TimeTicks::Now();
  ++arcvm_trim_count_;
}

// static
void WorkingSetTrimmerPolicyChromeOS::DoTrimArcVmOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* trimmer = static_cast<mechanism::WorkingSetTrimmerChromeOS*>(
      mechanism::WorkingSetTrimmer::GetInstance());
  trimmer->TrimArcVmWorkingSet(
      base::BindOnce(&OnTrimArcVmWorkingSetOnUIThread));
}

// static
size_t WorkingSetTrimmerPolicyChromeOS::GetArcVmTrimCountForFinalReport(
    size_t current_arcvm_trim_count,
    const base::TimeDelta& time_since_last_arcvm_trim_metric_report,
    const base::TimeDelta& arcvm_trim_backoff_time,
    const base::TimeDelta& arcvm_trim_metric_report_delay) {
  DCHECK_NE(0, time_since_last_arcvm_trim_metric_report.InMinutes());
  DCHECK_NE(0, arcvm_trim_backoff_time.InMinutes());

  // In |kArcVmTrimMetricReportDelay|, only |max_trim_count| times of ARCVM
  // trims can happen.
  const size_t max_trim_count = arcvm_trim_metric_report_delay.InMinutes() /
                                    arcvm_trim_backoff_time.InMinutes() +
                                1;

  // Adjust the |arcvm_trim_count_| before the final report. Use std::min() to
  // avoid reporting unrealistically large counts.
  return std::min<size_t>(
      current_arcvm_trim_count * arcvm_trim_metric_report_delay.InMinutes() /
          time_since_last_arcvm_trim_metric_report.InMinutes(),
      max_trim_count);
}

void WorkingSetTrimmerPolicyChromeOS::ReportArcVmTrimMetric() {
  base::UmaHistogramExactLinear("Memory.WorkingSetTrim.ArcVmTrimCountPer30Mins",
                                arcvm_trim_count_, kArcVmTrimMetricMaxCount);
  time_since_last_arcvm_trim_metric_report_ = base::ElapsedTimer();
  arcvm_trim_count_ = 0;
}

void WorkingSetTrimmerPolicyChromeOS::ReportArcVmTrimMetricOnDestruction() {
  if (!trim_arcvm_on_memory_pressure_)
    return;

  const base::TimeDelta elapsed =
      time_since_last_arcvm_trim_metric_report_.Elapsed();
  if (!elapsed.InMinutes())
    return;

  arcvm_trim_count_ = GetArcVmTrimCountForFinalReport(
      arcvm_trim_count_, elapsed, params_.arcvm_trim_backoff_time,
      kArcVmTrimMetricReportDelay);
  ReportArcVmTrimMetric();
}

void WorkingSetTrimmerPolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  memory_pressure_listener_.reset();
  arcvm_trim_metric_report_timer_.Stop();
  ReportArcVmTrimMetricOnDestruction();
  graph_ = nullptr;
  WorkingSetTrimmerPolicy::OnTakenFromGraph(graph);
}

void WorkingSetTrimmerPolicyChromeOS::OnAllFramesInProcessFrozen(
    const ProcessNode* process_node) {
  if (trim_on_freeze_) {
    WorkingSetTrimmerPolicy::OnAllFramesInProcessFrozen(process_node);
  }
}

void WorkingSetTrimmerPolicyChromeOS::OnPassedToGraph(Graph* graph) {
  if (trim_on_memory_pressure_ || trim_arc_on_memory_pressure_) {
    // We wait to register the memory pressure listener so we're on the
    // right sequence.
    params_ = features::TrimOnMemoryPressureParams::GetParams();
    memory_pressure_listener_.emplace(
        FROM_HERE,
        base::BindRepeating(&WorkingSetTrimmerPolicyChromeOS::OnMemoryPressure,
                            base::Unretained(this)));
  }
  if (trim_arcvm_on_memory_pressure_) {
    arcvm_trim_metric_report_timer_.Start(
        FROM_HERE, kArcVmTrimMetricReportDelay,
        base::BindRepeating(
            &WorkingSetTrimmerPolicyChromeOS::ReportArcVmTrimMetric,
            weak_ptr_factory_.GetWeakPtr()));
  }

  graph_ = graph;
  WorkingSetTrimmerPolicy::OnPassedToGraph(graph);
}

// static
bool WorkingSetTrimmerPolicyChromeOS::PlatformSupportsWorkingSetTrim() {
  return mechanism::WorkingSetTrimmer::GetInstance()
      ->PlatformSupportsWorkingSetTrim();
}

}  // namespace policies
}  // namespace performance_manager
