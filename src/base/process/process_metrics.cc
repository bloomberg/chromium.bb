// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace base {

namespace {

int CalculateEventsPerSecond(uint64_t event_count,
                             uint64_t* last_event_count,
                             base::TimeTicks* last_calculated) {
  const base::TimeTicks time = base::TimeTicks::Now();

  int events_per_second = 0;
  if (*last_event_count != 0) {
    const int64_t events_delta = event_count - *last_event_count;
    const base::TimeDelta time_delta = time - *last_calculated;
    DCHECK(!time_delta.is_zero());
    events_per_second = ClampRound(events_delta / time_delta.InSecondsF());
  }

  *last_calculated = time;
  *last_event_count = event_count;
  return events_per_second;
}

}  // namespace

SystemMemoryInfoKB::SystemMemoryInfoKB() = default;

SystemMemoryInfoKB::SystemMemoryInfoKB(const SystemMemoryInfoKB& other) =
    default;

SystemMetrics::SystemMetrics() {
  committed_memory_ = 0;
}

SystemMetrics SystemMetrics::Sample() {
  SystemMetrics system_metrics;

  system_metrics.committed_memory_ = GetSystemCommitCharge();
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  GetSystemMemoryInfo(&system_metrics.memory_info_);
  GetVmStatInfo(&system_metrics.vmstat_info_);
  GetSystemDiskInfo(&system_metrics.disk_info_);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  GetSwapInfo(&system_metrics.swap_info_);
  GetGraphicsMemoryInfo(&system_metrics.gpu_memory_info_);
#endif
#if defined(OS_WIN)
  GetSystemPerformanceInfo(&system_metrics.performance_);
#endif
  return system_metrics;
}

std::unique_ptr<Value> SystemMetrics::ToValue() const {
  std::unique_ptr<DictionaryValue> res(new DictionaryValue());

  res->SetIntKey("committed_memory", static_cast<int>(committed_memory_));
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  std::unique_ptr<DictionaryValue> meminfo = memory_info_.ToValue();
  std::unique_ptr<DictionaryValue> vmstat = vmstat_info_.ToValue();
  meminfo->MergeDictionary(vmstat.get());
  res->Set("meminfo", std::move(meminfo));
  res->Set("diskinfo", disk_info_.ToValue());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  res->Set("swapinfo", swap_info_.ToValue());
  res->Set("gpu_meminfo", gpu_memory_info_.ToValue());
#endif
#if defined(OS_WIN)
  res->Set("perfinfo", performance_.ToValue());
#endif

  return std::move(res);
}

std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateCurrentProcessMetrics() {
#if !defined(OS_MAC)
  return CreateProcessMetrics(base::GetCurrentProcessHandle());
#else
  return CreateProcessMetrics(base::GetCurrentProcessHandle(), nullptr);
#endif  // !defined(OS_MAC)
}

#if !defined(OS_FREEBSD) || !defined(OS_POSIX)
double ProcessMetrics::GetPlatformIndependentCPUUsage() {
  TimeDelta cumulative_cpu = GetCumulativeCPUUsage();
  TimeTicks time = TimeTicks::Now();

  if (last_cumulative_cpu_.is_zero()) {
    // First call, just set the last values.
    last_cumulative_cpu_ = cumulative_cpu;
    last_cpu_time_ = time;
    return 0;
  }

  TimeDelta system_time_delta = cumulative_cpu - last_cumulative_cpu_;
  TimeDelta time_delta = time - last_cpu_time_;
  DCHECK(!time_delta.is_zero());
  if (time_delta.is_zero())
    return 0;

  last_cumulative_cpu_ = cumulative_cpu;
  last_cpu_time_ = time;

  return 100.0 * system_time_delta / time_delta;
}
#endif

#if defined(OS_APPLE) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
int ProcessMetrics::CalculateIdleWakeupsPerSecond(
    uint64_t absolute_idle_wakeups) {
  return CalculateEventsPerSecond(absolute_idle_wakeups,
                                  &last_absolute_idle_wakeups_,
                                  &last_idle_wakeups_time_);
}
#else
int ProcessMetrics::GetIdleWakeupsPerSecond() {
  NOTIMPLEMENTED();  // http://crbug.com/120488
  return 0;
}
#endif  // defined(OS_APPLE) || defined(OS_LINUX) || defined(OS_CHROMEOS) ||
        // defined(OS_AIX)

#if defined(OS_APPLE)
int ProcessMetrics::CalculatePackageIdleWakeupsPerSecond(
    uint64_t absolute_package_idle_wakeups) {
  return CalculateEventsPerSecond(absolute_package_idle_wakeups,
                                  &last_absolute_package_idle_wakeups_,
                                  &last_package_idle_wakeups_time_);
}

#endif  // defined(OS_APPLE)

#if !defined(OS_WIN)
uint64_t ProcessMetrics::GetCumulativeDiskUsageInBytes() {
  // Not implemented.
  return 0;
}
#endif

uint64_t ProcessMetrics::GetDiskUsageBytesPerSecond() {
  uint64_t cumulative_disk_usage = GetCumulativeDiskUsageInBytes();
  return CalculateEventsPerSecond(cumulative_disk_usage,
                                  &last_cumulative_disk_usage_,
                                  &last_disk_usage_time_);
}

}  // namespace base
