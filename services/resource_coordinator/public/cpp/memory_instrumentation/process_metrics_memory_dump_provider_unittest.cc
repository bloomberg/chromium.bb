// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/process_metrics_memory_dump_provider.h"

#include <stdint.h>

#include <memory>
#include <unordered_set>

#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"
#include "base/trace_event/process_memory_totals.h"
#include "base/trace_event/trace_event_argument.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

#if defined(OS_LINUX) || defined(OS_ANDROID)
namespace {
const char kTestStatm[] = "200 100 20 2 3 4";

void CreateTempFileWithContents(const char* contents, base::ScopedFILE* file) {
  base::FilePath temp_path;
  FILE* temp_file = CreateAndOpenTemporaryFile(&temp_path);
  file->reset(temp_file);
  ASSERT_TRUE(temp_file);

  ASSERT_TRUE(
      base::WriteFileDescriptor(fileno(temp_file), contents, strlen(contents)));
}

}  // namespace
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

class MockMemoryDumpProvider : public ProcessMetricsMemoryDumpProvider {
 public:
  MockMemoryDumpProvider(base::ProcessId process);
  ~MockMemoryDumpProvider() override;
};

std::unordered_set<MockMemoryDumpProvider*> g_live_mocks;
std::unordered_set<MockMemoryDumpProvider*> g_dead_mocks;

MockMemoryDumpProvider::MockMemoryDumpProvider(base::ProcessId process)
    : ProcessMetricsMemoryDumpProvider(process) {
  g_live_mocks.insert(this);
}

MockMemoryDumpProvider::~MockMemoryDumpProvider() {
  g_live_mocks.erase(this);
  g_dead_mocks.insert(this);
}

TEST(ProcessMetricsMemoryDumpProviderTest, DumpRSS) {
  const base::trace_event::MemoryDumpArgs args = {
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND};
  std::unique_ptr<ProcessMetricsMemoryDumpProvider> pmtdp(
      new ProcessMetricsMemoryDumpProvider(base::kNullProcessId));
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd_before(
      new base::trace_event::ProcessMemoryDump(nullptr, args));
  std::unique_ptr<base::trace_event::ProcessMemoryDump> pmd_after(
      new base::trace_event::ProcessMemoryDump(nullptr, args));

  ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing = 1024;
  pmtdp->OnMemoryDump(args, pmd_before.get());

  // Pretend that the RSS of the process increased of +1M.
  const size_t kAllocSize = 1048576;
  ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing += kAllocSize;

  pmtdp->OnMemoryDump(args, pmd_after.get());

  ProcessMetricsMemoryDumpProvider::rss_bytes_for_testing = 0;

  ASSERT_TRUE(pmd_before->has_process_totals());
  ASSERT_TRUE(pmd_after->has_process_totals());

  const uint64_t rss_before =
      pmd_before->process_totals()->resident_set_bytes();
  const uint64_t rss_after = pmd_after->process_totals()->resident_set_bytes();

  EXPECT_NE(0U, rss_before);
  EXPECT_NE(0U, rss_after);

  EXPECT_EQ(rss_after - rss_before, kAllocSize);
}

#if defined(OS_LINUX) || defined(OS_ANDROID)

TEST(ProcessMetricsMemoryDumpProviderTest, DoubleRegister) {
  auto factory = [](base::ProcessId process) {
    return std::unique_ptr<ProcessMetricsMemoryDumpProvider>(
        new MockMemoryDumpProvider(process));
  };
  ProcessMetricsMemoryDumpProvider::factory_for_testing = factory;
  ProcessMetricsMemoryDumpProvider::RegisterForProcess(1);
  ProcessMetricsMemoryDumpProvider::RegisterForProcess(1);
  ASSERT_EQ(1u, g_live_mocks.size());
  ASSERT_EQ(1u, g_dead_mocks.size());
  auto* manager = base::trace_event::MemoryDumpManager::GetInstance();
  MockMemoryDumpProvider* live_mock = *g_live_mocks.begin();
  EXPECT_TRUE(manager->IsDumpProviderRegisteredForTesting(live_mock));
  auto* dead_mock = *g_dead_mocks.begin();
  EXPECT_FALSE(manager->IsDumpProviderRegisteredForTesting(dead_mock));
  ProcessMetricsMemoryDumpProvider::UnregisterForProcess(1);
  g_live_mocks.clear();
  g_dead_mocks.clear();
}

#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

TEST(ProcessMetricsMemoryDumpProviderTest, TestPollFastMemoryTotal) {
  ProcessMetricsMemoryDumpProvider mdp(base::kNullProcessId);

  uint64_t total1, total2;
  mdp.PollFastMemoryTotal(&total1);
  ASSERT_GT(total1, 0u);
  size_t kBufSize = 16 * 1024 * 1024;
  auto buf = base::MakeUnique<char[]>(kBufSize);
  for (size_t i = 0; i < kBufSize; i++)
    buf[i] = *((volatile char*)&buf[i]) + 1;
  mdp.PollFastMemoryTotal(&total2);
  ASSERT_GT(total2, 0u);
  EXPECT_GT(total2, total1 + kBufSize / 2);

#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_GE(mdp.fast_polling_statm_fd_.get(), 0);

  base::ScopedFILE temp_file;
  CreateTempFileWithContents(kTestStatm, &temp_file);
  mdp.fast_polling_statm_fd_for_testing = fileno(temp_file.get());
  size_t page_size = base::GetPageSize();
  uint64_t value;
  mdp.PollFastMemoryTotal(&value);
  EXPECT_EQ(100 * page_size, value);

  mdp.SuspendFastMemoryPolling();
  EXPECT_FALSE(mdp.fast_polling_statm_fd_.is_valid());
#else
  mdp.SuspendFastMemoryPolling();
#endif
}

}  // namespace memory_instrumentation
