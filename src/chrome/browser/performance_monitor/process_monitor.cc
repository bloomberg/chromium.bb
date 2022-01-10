// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_monitor.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/process/process_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/process_metrics_history.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/manifest_handlers/background_info.h"
#endif

#if defined(OS_WIN)
#include "sandbox/policy/mojom/sandbox.mojom-shared.h"
#endif

using content::BrowserThread;

namespace performance_monitor {

namespace {

// The global instance.
ProcessMonitor* g_process_monitor = nullptr;

void GatherMetricsForRenderProcess(content::RenderProcessHost* host,
                                   ProcessMetadata* data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::BrowserContext* browser_context = host->GetBrowserContext();
  extensions::ProcessMap* extension_process_map =
      extensions::ProcessMap::Get(browser_context);

  std::set<std::string> extension_ids =
      extension_process_map->GetExtensionsInProcess(host->GetID());

  // We only collect more granular metrics when there's only one extension
  // running in a given renderer, to reduce noise.
  if (extension_ids.size() != 1)
    return;

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);

  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(*extension_ids.begin());

  if (!extension)
    return;

  data->process_subtype =
      extensions::BackgroundInfo::HasPersistentBackgroundPage(extension)
          ? kProcessSubtypeExtensionPersistent
          : kProcessSubtypeExtensionEvent;
#endif
}

// Adds the values from |rhs| to |lhs|.
ProcessMonitor::Metrics& operator+=(ProcessMonitor::Metrics& lhs,
                                    const ProcessMonitor::Metrics& rhs) {
  lhs.cpu_usage += rhs.cpu_usage;

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
  lhs.idle_wakeups += rhs.idle_wakeups;
#endif

#if defined(OS_MAC)
  lhs.package_idle_wakeups += rhs.package_idle_wakeups;
  lhs.energy_impact += rhs.energy_impact;
#endif

  return lhs;
}

}  // namespace

constexpr base::TimeDelta ProcessMonitor::kGatherInterval;

ProcessMonitor::Metrics::Metrics() = default;
ProcessMonitor::Metrics::Metrics(const ProcessMonitor::Metrics& other) =
    default;
ProcessMonitor::Metrics& ProcessMonitor::Metrics::operator=(
    const ProcessMonitor::Metrics& other) = default;
ProcessMonitor::Metrics::~Metrics() = default;

// static
std::unique_ptr<ProcessMonitor> ProcessMonitor::Create() {
  DCHECK(!g_process_monitor);
  return base::WrapUnique(new ProcessMonitor());
}

// static
ProcessMonitor* ProcessMonitor::Get() {
  return g_process_monitor;
}

ProcessMonitor::~ProcessMonitor() {
  DCHECK(g_process_monitor);
  g_process_monitor = nullptr;
}

void ProcessMonitor::StartGatherCycle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Start(FROM_HERE, kGatherInterval, this,
                         &ProcessMonitor::GatherProcesses);
}

void ProcessMonitor::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ProcessMonitor::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

ProcessMonitor::ProcessMonitor() {
  DCHECK(!g_process_monitor);
  g_process_monitor = this;
}

void ProcessMonitor::MarkProcessAsAlive(const ProcessMetadata& process_data,
                                        int current_update_sequence) {
  const base::ProcessHandle& handle = process_data.handle;
  if (handle == base::kNullProcessHandle) {
    // Process may not be valid yet.
    return;
  }

  auto process_metrics_iter = metrics_map_.find(handle);
  if (process_metrics_iter == metrics_map_.end()) {
    // If we're not already watching the process, let's initialize it.
    metrics_map_[handle] = std::make_unique<ProcessMetricsHistory>();
    metrics_map_[handle]->Initialize(process_data, current_update_sequence);
  } else {
    // If we are watching the process, touch it to keep it alive.
    ProcessMetricsHistory* process_metrics = process_metrics_iter->second.get();
    process_metrics->set_last_update_sequence(current_update_sequence);
  }
}

// static
std::vector<ProcessMetadata> ProcessMonitor::GatherRendererProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<ProcessMetadata> processes;

  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    content::RenderProcessHost* host = rph_iter.GetCurrentValue();
    ProcessMetadata data;
    data.process_type = content::PROCESS_TYPE_RENDERER;
    data.handle = host->GetProcess().Handle();

    GatherMetricsForRenderProcess(host, &data);

    processes.push_back(data);
  }

  return processes;
}

// static
std::vector<ProcessMetadata> ProcessMonitor::GatherNonRendererProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<ProcessMetadata> processes;

  // Find all child processes (does not include renderers), which has to be
  // done on the IO thread.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
#if defined(OS_WIN)
    // Cannot gather process metrics for elevated process as browser has no
    // access to them.
    if (iter.GetData().sandbox_type ==
        sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
      continue;
    }
#endif
    ProcessMetadata child_process_data;
    child_process_data.handle = iter.GetData().GetProcess().Handle();
    child_process_data.process_type = iter.GetData().process_type;

    if (iter.GetData().metrics_name == network::mojom::NetworkService::Name_) {
      child_process_data.process_subtype = kProcessSubtypeNetworkProcess;
    }

    processes.push_back(child_process_data);
  }

  // Add the current (browser) process.
  ProcessMetadata browser_process_data;
  browser_process_data.process_type = content::PROCESS_TYPE_BROWSER;
  browser_process_data.handle = base::GetCurrentProcessHandle();

  processes.push_back(browser_process_data);

  return processes;
}

void ProcessMonitor::GatherProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static uint32_t current_update_sequence = 0;
  // Even in the "somewhat" unlikely event this wraps around,
  // it doesn't matter. We just check it for inequality.
  current_update_sequence++;

  std::vector<ProcessMetadata> processes = GatherRendererProcesses();
  auto non_renderers = GatherNonRendererProcesses();
  processes.insert(processes.end(), non_renderers.begin(), non_renderers.end());

  for (const auto& process : processes)
    MarkProcessAsAlive(process, current_update_sequence);

  // Update metrics for all watched processes; remove dead entries from the map.
  Metrics aggregated_metrics;
  auto iter = metrics_map_.begin();
  while (iter != metrics_map_.end()) {
    ProcessMetricsHistory* process_metrics = iter->second.get();
    if (process_metrics->last_update_sequence() !=
        static_cast<int>(current_update_sequence)) {
      // Not touched this iteration; let's get rid of it.
      metrics_map_.erase(iter++);
    } else {
      Metrics metrics = process_metrics->SampleMetrics();
      aggregated_metrics += metrics;
      for (auto& observer : observer_list_)
        observer.OnMetricsSampled(process_metrics->metadata(), metrics);
      ++iter;
    }
  }

#if defined(OS_MAC)
  if (coalition_data_provider_.IsAvailable())
    aggregated_metrics.coalition_data = coalition_data_provider_.GetDataRate();
#endif

  for (auto& observer : observer_list_)
    observer.OnAggregatedMetricsSampled(aggregated_metrics);
}

}  // namespace performance_monitor
