// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_export.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace base {
class ProcessMetrics;
}

namespace memory_instrumentation {

// Dump provider which collects process-wide memory stats.
class SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_EXPORT
    ProcessMetricsMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Pass base::kNullProcessId to register for current process.
  static void RegisterForProcess(base::ProcessId process);
  static void UnregisterForProcess(base::ProcessId process);

  ~ProcessMetricsMemoryDumpProvider() override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;
  void PollFastMemoryTotal(uint64_t* memory_total) override;
  void SuspendFastMemoryPolling() override;

 protected:
  ProcessMetricsMemoryDumpProvider(base::ProcessId process);

 private:
  using FactoryFunction =
      std::unique_ptr<ProcessMetricsMemoryDumpProvider> (*)(base::ProcessId);

  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           ParseProcSmaps);
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest, DumpRSS);
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           TestPollFastMemoryTotal);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           TestMachOReading);
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           NoDuplicateRegions);
#elif defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           TestWinModuleReading);
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  FRIEND_TEST_ALL_PREFIXES(ProcessMetricsMemoryDumpProviderTest,
                           DoubleRegister);
#endif

  bool DumpProcessTotals(const base::trace_event::MemoryDumpArgs& args,
                         base::trace_event::ProcessMemoryDump* pmd);
  bool DumpProcessMemoryMaps(const base::trace_event::MemoryDumpArgs& args,
                             base::trace_event::ProcessMemoryDump* pmd);
  uint64_t GetPeakResidentSetBytes();

  static uint64_t rss_bytes_for_testing;
  static FactoryFunction factory_for_testing;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  static FILE* proc_smaps_for_testing;
  static int fast_polling_statm_fd_for_testing;

  base::ScopedFD fast_polling_statm_fd_;

  base::ScopedFD OpenStatm();
#endif

  base::ProcessId process_;
  std::unique_ptr<base::ProcessMetrics> process_metrics_;

  // The peak may not be resettable on all the processes if the linux kernel is
  // older than http://bit.ly/reset_rss or only on child processes if yama LSM
  // sandbox is enabled.
  bool is_rss_peak_resettable_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMetricsMemoryDumpProvider);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_PROCESS_MEMORY_METRICS_DUMP_PROVIDER_H_
