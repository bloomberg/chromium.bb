// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/arc_shared_sampler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/chromeos/arc/process/arc_process_service.h"
#include "content/public/browser/browser_thread.h"

namespace task_manager {

namespace {
enum MemoryDumpType {
  kAppMemoryDump = 1 << 0,
  kSystemMemoryDump = 1 << 1,
};

// The minimum amount of time between calls to ArcProcessService.
constexpr base::TimeDelta kAppThrottleLimit = base::TimeDelta::FromSeconds(2);
constexpr base::TimeDelta kSystemThrottleLimit =
    base::TimeDelta::FromSeconds(3);
};  // namespace

ArcSharedSampler::ArcSharedSampler() : weak_ptr_factory_(this) {}

ArcSharedSampler::~ArcSharedSampler() = default;

void ArcSharedSampler::RegisterCallback(
    base::ProcessId process_id,
    OnSamplingCompleteCallback on_sampling_complete) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(process_id, base::kNullProcessId);
  const bool result =
      callbacks_.emplace(process_id, std::move(on_sampling_complete)).second;
  DCHECK(result);
}

void ArcSharedSampler::UnregisterCallback(base::ProcessId process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.erase(process_id);
}

void ArcSharedSampler::Refresh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcProcessService* arc_process_service = arc::ArcProcessService::Get();
  if (!arc_process_service)
    return;
  const base::TimeDelta time_since_app_refresh =
      base::Time::Now() - last_app_refresh;
  const base::TimeDelta time_since_system_refresh =
      base::Time::Now() - last_system_refresh;
  if ((~pending_memory_dump_types_ & MemoryDumpType::kAppMemoryDump) &&
      time_since_app_refresh > kAppThrottleLimit) {
    arc_process_service->RequestAppMemoryInfo(base::BindOnce(
        &ArcSharedSampler::OnReceiveMemoryDump, weak_ptr_factory_.GetWeakPtr(),
        MemoryDumpType::kAppMemoryDump));
    pending_memory_dump_types_ |= MemoryDumpType::kAppMemoryDump;
  }
  if ((~pending_memory_dump_types_ & MemoryDumpType::kSystemMemoryDump) &&
      time_since_system_refresh > kSystemThrottleLimit) {
    arc_process_service->RequestSystemMemoryInfo(base::BindOnce(
        &ArcSharedSampler::OnReceiveMemoryDump, weak_ptr_factory_.GetWeakPtr(),
        MemoryDumpType::kSystemMemoryDump));
    pending_memory_dump_types_ |= MemoryDumpType::kSystemMemoryDump;
  }
}

void ArcSharedSampler::OnReceiveMemoryDump(
    int type,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_memory_dump_types_ &= ~type;

  if (type == MemoryDumpType::kAppMemoryDump)
    last_app_refresh = base::Time::Now();
  else
    last_system_refresh = base::Time::Now();

  if (!dump)
    return;
  for (const auto& pmd : dump->process_dumps()) {
    auto it = callbacks_.find(pmd.pid());
    if (it == callbacks_.end())
      continue;
    const MemoryFootprintBytes result =
        pmd.os_dump().private_footprint_kb * 1024;
    it->second.Run(base::make_optional<MemoryFootprintBytes>(result));
  }
}

}  // namespace task_manager
