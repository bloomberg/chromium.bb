// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/process_metrics_memory_dump_provider.h"

#include <fcntl.h>
#include <stdint.h>

#include <map>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace memory_instrumentation {

namespace {

base::LazyInstance<
    std::map<base::ProcessId,
             std::unique_ptr<ProcessMetricsMemoryDumpProvider>>>::Leaky
    g_dump_providers_map = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_LINUX) || defined(OS_ANDROID)

const char kClearPeakRssCommand[] = "5";
const uint32_t kMaxLineSize = 4096;

#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessId process) {
  if (process == base::kNullProcessId)
    return base::ProcessMetrics::CreateCurrentProcessMetrics();
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // Just pass ProcessId instead of handle since they are the same in linux and
  // android.
  return base::ProcessMetrics::CreateProcessMetrics(process);
#else
  // Creating process metrics for child processes in mac or windows requires
  // additional information like ProcessHandle or port provider.
  NOTREACHED();
  return std::unique_ptr<base::ProcessMetrics>();
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
}

}  // namespace

// static
uint64_t ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing = 0;

// static
ProcessMetricsMemoryDumpProvider::FactoryFunction
    ProcessMetricsMemoryDumpProvider::factory_for_testing = nullptr;

// static
void ProcessMetricsMemoryDumpProvider::RegisterForProcess(
    base::ProcessId process) {
  std::unique_ptr<ProcessMetricsMemoryDumpProvider> owned_provider;
  if (factory_for_testing) {
    owned_provider = factory_for_testing(process);
  } else {
    owned_provider = std::unique_ptr<ProcessMetricsMemoryDumpProvider>(
        new ProcessMetricsMemoryDumpProvider(process));
  }

  ProcessMetricsMemoryDumpProvider* provider = owned_provider.get();
  bool did_insert =
      g_dump_providers_map.Get()
          .insert(std::make_pair(process, std::move(owned_provider)))
          .second;
  if (!did_insert) {
    DLOG(ERROR) << "ProcessMetricsMemoryDumpProvider already registered for "
                << (process == base::kNullProcessId
                        ? "current process"
                        : "process id " + base::IntToString(process));
    return;
  }
  base::trace_event::MemoryDumpProvider::Options options;
  options.target_pid = process;
  options.is_fast_polling_supported = true;
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      provider, "ProcessMemoryMetrics", nullptr, options);
}

// static
void ProcessMetricsMemoryDumpProvider::UnregisterForProcess(
    base::ProcessId process) {
  auto iter = g_dump_providers_map.Get().find(process);
  if (iter == g_dump_providers_map.Get().end())
    return;
  base::trace_event::MemoryDumpManager::GetInstance()
      ->UnregisterAndDeleteDumpProviderSoon(std::move(iter->second));
  g_dump_providers_map.Get().erase(iter);
}

ProcessMetricsMemoryDumpProvider::ProcessMetricsMemoryDumpProvider(
    base::ProcessId process)
    : process_(process),
      process_metrics_(CreateProcessMetrics(process)),
      is_rss_peak_resettable_(true) {}

ProcessMetricsMemoryDumpProvider::~ProcessMetricsMemoryDumpProvider() {}

// Called at trace dump point time. Creates a snapshot of the memory maps for
// the current process.
bool ProcessMetricsMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  bool res = DumpProcessTotals(args, pmd);

  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED)
    res &= DumpProcessMemoryMaps(args, pmd);
  return res;
}

bool ProcessMetricsMemoryDumpProvider::DumpProcessMemoryMaps(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  return OSMetrics::FillProcessMemoryMapsDeprecated(process_, pmd);
}

bool ProcessMetricsMemoryDumpProvider::DumpProcessTotals(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
// On Mac set a few extra values on process_totals:
#if defined(OS_MACOSX)
  size_t private_bytes;
  size_t shared_bytes;
  size_t resident_bytes;
  size_t locked_bytes;
  if (!process_metrics_->GetMemoryBytes(&private_bytes, &shared_bytes,
                                        &resident_bytes, &locked_bytes)) {
    return false;
  }
  pmd->process_totals()->SetExtraFieldInBytes("private_bytes", private_bytes);
  pmd->process_totals()->SetExtraFieldInBytes("shared_bytes", shared_bytes);
  pmd->process_totals()->SetExtraFieldInBytes("locked_bytes", locked_bytes);
#endif  // defined(OS_MACOSX)

  mojom::RawOSMemDump dump;
  if (!OSMetrics::FillOSMemoryDump(process_, &dump)) {
    return false;
  }

  uint64_t rss_bytes = dump.resident_set_kb * 1024;
  if (rss_bytes_for_testing) {
    rss_bytes = rss_bytes_for_testing;
  }

  // resident set size will be 0 if the process ended while dumping.
  if (rss_bytes == 0u) {
    return false;
  }

  pmd->process_totals()->set_resident_set_bytes(rss_bytes);
  pmd->process_totals()->SetPlatformPrivateFootprint(
      dump.platform_private_footprint);
  pmd->set_has_process_totals();
  pmd->process_totals()->set_peak_resident_set_bytes(GetPeakResidentSetBytes());

// On Linux and Android reset rss peak if necessary
#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (is_rss_peak_resettable_) {
    std::string clear_refs_file =
        "/proc/" +
        (process_ == base::kNullProcessId ? "self"
                                          : base::IntToString(process_)) +
        "/clear_refs";
    int clear_refs_fd = open(clear_refs_file.c_str(), O_WRONLY);
    if (clear_refs_fd > 0 &&
        base::WriteFileDescriptor(clear_refs_fd, kClearPeakRssCommand,
                                  sizeof(kClearPeakRssCommand))) {
      pmd->process_totals()->set_is_peak_rss_resetable(true);
    } else {
      is_rss_peak_resettable_ = false;
    }
    close(clear_refs_fd);
  }
#endif

  // Returns true even if other metrics failed, since rss is reported.
  return true;
}

uint64_t ProcessMetricsMemoryDumpProvider::GetPeakResidentSetBytes() {
#if defined(OS_IOS)
  return 0u;
#else
  return process_metrics_->GetPeakWorkingSetSize();
#endif
}

#if defined(OS_LINUX) || defined(OS_ANDROID)

// static
int ProcessMetricsMemoryDumpProvider::fast_polling_statm_fd_for_testing = -1;

base::ScopedFD ProcessMetricsMemoryDumpProvider::OpenStatm() {
  std::string name =
      "/proc/" +
      (process_ == base::kNullProcessId ? "self"
                                        : base::IntToString(process_)) +
      "/statm";
  base::ScopedFD fd = base::ScopedFD(open(name.c_str(), O_RDONLY));
  DCHECK(fd.is_valid());
  return fd;
}

bool GetResidentPagesFromStatmFile(int fd, uint64_t* resident_pages) {
  lseek(fd, 0, SEEK_SET);
  char line[kMaxLineSize];
  int res = read(fd, line, kMaxLineSize - 1);
  if (res <= 0)
    return false;
  line[res] = '\0';
  int num_scanned = sscanf(line, "%*s %" SCNu64, resident_pages);
  return num_scanned == 1;
}

#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

void ProcessMetricsMemoryDumpProvider::PollFastMemoryTotal(
    uint64_t* memory_total) {
  *memory_total = 0;
#if defined(OS_LINUX) || defined(OS_ANDROID)

  int statm_fd = fast_polling_statm_fd_for_testing;
  if (statm_fd == -1) {
    if (!fast_polling_statm_fd_.is_valid())
      fast_polling_statm_fd_ = OpenStatm();
    statm_fd = fast_polling_statm_fd_.get();
    if (statm_fd == -1)
      return;
  }

  uint64_t resident_pages = 0;
  if (!GetResidentPagesFromStatmFile(statm_fd, &resident_pages))
    return;

  static size_t page_size = base::GetPageSize();
  *memory_total = resident_pages * page_size;
#else
  *memory_total = process_metrics_->GetWorkingSetSize();
#endif
}

void ProcessMetricsMemoryDumpProvider::SuspendFastMemoryPolling() {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  fast_polling_statm_fd_.reset();
#endif
}

}  // namespace memory_instrumentation
