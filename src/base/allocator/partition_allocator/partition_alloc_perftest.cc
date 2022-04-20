// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

#if BUILDFLAG(IS_ANDROID) || defined(ARCH_CPU_32_BITS) || BUILDFLAG(IS_FUCHSIA)
// Some tests allocate many GB of memory, which can cause issues on Android and
// address-space exhaustion for any 32-bit process.
#define MEMORY_CONSTRAINED
#endif

namespace partition_alloc::internal {

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::base::BindOnce;
using ::base::OnceCallback;
using ::base::StringPrintf;
using ::base::Unretained;

}  // namespace base

namespace {

// Change kTimeLimit to something higher if you need more time to capture a
// trace.
constexpr base::TimeDelta kTimeLimit = base::Seconds(2);
constexpr int kWarmupRuns = 10000;
constexpr int kTimeCheckInterval = 100000;
constexpr size_t kAllocSize = 40;

// Size constants are mostly arbitrary, but try to simulate something like CSS
// parsing which consists of lots of relatively small objects.
constexpr int kMultiBucketMinimumSize = 24;
constexpr int kMultiBucketIncrement = 13;
// Final size is 24 + (13 * 22) = 310 bytes.
constexpr int kMultiBucketRounds = 22;

constexpr char kMetricPrefixMemoryAllocation[] = "MemoryAllocation.";
constexpr char kMetricThroughput[] = "throughput";
constexpr char kMetricTimePerAllocation[] = "time_per_allocation";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixMemoryAllocation,
                                         story_name);
  reporter.RegisterImportantMetric(kMetricThroughput, "runs/s");
  reporter.RegisterImportantMetric(kMetricTimePerAllocation, "ns");
  return reporter;
}

enum class AllocatorType {
  kSystem,
  kPartitionAlloc,
  kPartitionAllocWithThreadCache
};

class Allocator {
 public:
  Allocator() = default;
  virtual ~Allocator() = default;
  virtual void* Alloc(size_t size) = 0;
  virtual void Free(void* data) = 0;
};

class SystemAllocator : public Allocator {
 public:
  SystemAllocator() = default;
  ~SystemAllocator() override = default;
  void* Alloc(size_t size) override { return malloc(size); }
  void Free(void* data) override { free(data); }
};

class PartitionAllocator : public Allocator {
 public:
  PartitionAllocator() = default;
  ~PartitionAllocator() override { alloc_.DestructForTesting(); }

  void* Alloc(size_t size) override {
    return alloc_.AllocWithFlagsNoHooks(0, size, PartitionPageSize());
  }
  void Free(void* data) override { ThreadSafePartitionRoot::FreeNoHooks(data); }

 private:
  ThreadSafePartitionRoot alloc_{{
      PartitionOptions::AlignedAlloc::kDisallowed,
      PartitionOptions::ThreadCache::kDisabled,
      PartitionOptions::Quarantine::kDisallowed,
      PartitionOptions::Cookie::kAllowed,
      PartitionOptions::BackupRefPtr::kDisabled,
      PartitionOptions::UseConfigurablePool::kNo,
  }};
};

// Only one partition with a thread cache.
ThreadSafePartitionRoot* g_partition_root = nullptr;
class PartitionAllocatorWithThreadCache : public Allocator {
 public:
  explicit PartitionAllocatorWithThreadCache(bool use_alternate_bucket_dist) {
    if (!g_partition_root) {
      g_partition_root = new ThreadSafePartitionRoot({
          PartitionOptions::AlignedAlloc::kDisallowed,
          PartitionOptions::ThreadCache::kEnabled,
          PartitionOptions::Quarantine::kDisallowed,
          PartitionOptions::Cookie::kAllowed,
          PartitionOptions::BackupRefPtr::kDisabled,
          PartitionOptions::UseConfigurablePool::kNo,
      });
    }
    ThreadCacheRegistry::Instance().PurgeAll();
    if (!use_alternate_bucket_dist)
      g_partition_root->SwitchToDenserBucketDistribution();
    else
      g_partition_root->ResetBucketDistributionForTesting();
  }
  ~PartitionAllocatorWithThreadCache() override = default;

  void* Alloc(size_t size) override {
    return g_partition_root->AllocWithFlagsNoHooks(0, size,
                                                   PartitionPageSize());
  }
  void Free(void* data) override { ThreadSafePartitionRoot::FreeNoHooks(data); }
};

class TestLoopThread : public base::PlatformThread::Delegate {
 public:
  explicit TestLoopThread(base::OnceCallback<float()> test_fn)
      : test_fn_(std::move(test_fn)) {
    PA_CHECK(base::PlatformThread::Create(0, this, &thread_handle_));
  }

  float Run() {
    base::PlatformThread::Join(thread_handle_);
    return laps_per_second_;
  }

  void ThreadMain() override { laps_per_second_ = std::move(test_fn_).Run(); }

  base::OnceCallback<float()> test_fn_;
  base::PlatformThreadHandle thread_handle_;
  std::atomic<float> laps_per_second_;
};

void DisplayResults(const std::string& story_name,
                    float iterations_per_second) {
  auto reporter = SetUpReporter(story_name);
  reporter.AddResult(kMetricThroughput, iterations_per_second);
  reporter.AddResult(kMetricTimePerAllocation,
                     static_cast<size_t>(1e9 / iterations_per_second));
}

class MemoryAllocationPerfNode {
 public:
  MemoryAllocationPerfNode* GetNext() const { return next_; }
  void SetNext(MemoryAllocationPerfNode* p) { next_ = p; }
  static void FreeAll(MemoryAllocationPerfNode* first, Allocator* alloc) {
    MemoryAllocationPerfNode* cur = first;
    while (cur != nullptr) {
      MemoryAllocationPerfNode* next = cur->GetNext();
      alloc->Free(cur);
      cur = next;
    }
  }

 private:
  MemoryAllocationPerfNode* next_ = nullptr;
};

#if !defined(MEMORY_CONSTRAINED)
float SingleBucket(Allocator* allocator) {
  auto* first =
      reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(kAllocSize));
  size_t allocated_memory = kAllocSize;

  base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  MemoryAllocationPerfNode* cur = first;
  do {
    auto* next = reinterpret_cast<MemoryAllocationPerfNode*>(
        allocator->Alloc(kAllocSize));
    CHECK_NE(next, nullptr);
    cur->SetNext(next);
    cur = next;
    timer.NextLap();
    allocated_memory += kAllocSize;
    // With multiple threads, can get OOM otherwise.
    if (allocated_memory > 200e6) {
      cur->SetNext(nullptr);
      MemoryAllocationPerfNode::FreeAll(first->GetNext(), allocator);
      cur = first;
      allocated_memory = kAllocSize;
    }
  } while (!timer.HasTimeLimitExpired());

  // next_ = nullptr only works if the class constructor is called (it's not
  // called in this case because then we can allocate arbitrary-length
  // payloads.)
  cur->SetNext(nullptr);
  MemoryAllocationPerfNode::FreeAll(first, allocator);

  return timer.LapsPerSecond();
}
#endif  // defined(MEMORY_CONSTRAINED)

float SingleBucketWithFree(Allocator* allocator) {
  // Allocate an initial element to make sure the bucket stays set up.
  void* elem = allocator->Alloc(kAllocSize);

  base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    void* cur = allocator->Alloc(kAllocSize);
    CHECK_NE(cur, nullptr);
    allocator->Free(cur);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  allocator->Free(elem);
  return timer.LapsPerSecond();
}

#if !defined(MEMORY_CONSTRAINED)
float MultiBucket(Allocator* allocator) {
  auto* first =
      reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(kAllocSize));
  MemoryAllocationPerfNode* cur = first;
  size_t allocated_memory = kAllocSize;

  base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    for (int i = 0; i < kMultiBucketRounds; i++) {
      size_t size = kMultiBucketMinimumSize + (i * kMultiBucketIncrement);
      auto* next =
          reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(size));
      CHECK_NE(next, nullptr);
      cur->SetNext(next);
      cur = next;
      allocated_memory += size;
    }

    // Can OOM with multiple threads.
    if (allocated_memory > 100e6) {
      cur->SetNext(nullptr);
      MemoryAllocationPerfNode::FreeAll(first->GetNext(), allocator);
      cur = first;
      allocated_memory = kAllocSize;
    }

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  cur->SetNext(nullptr);
  MemoryAllocationPerfNode::FreeAll(first, allocator);

  return timer.LapsPerSecond() * kMultiBucketRounds;
}
#endif  // defined(MEMORY_CONSTRAINED)

float MultiBucketWithFree(Allocator* allocator) {
  std::vector<void*> elems;
  elems.reserve(kMultiBucketRounds);
  // Do an initial round of allocation to make sure that the buckets stay in
  // use (and aren't accidentally released back to the OS).
  for (int i = 0; i < kMultiBucketRounds; i++) {
    void* cur =
        allocator->Alloc(kMultiBucketMinimumSize + (i * kMultiBucketIncrement));
    CHECK_NE(cur, nullptr);
    elems.push_back(cur);
  }

  base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    for (int i = 0; i < kMultiBucketRounds; i++) {
      void* cur = allocator->Alloc(kMultiBucketMinimumSize +
                                   (i * kMultiBucketIncrement));
      CHECK_NE(cur, nullptr);
      allocator->Free(cur);
    }
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  for (void* ptr : elems) {
    allocator->Free(ptr);
  }

  return timer.LapsPerSecond() * kMultiBucketRounds;
}

float DirectMapped(Allocator* allocator) {
  constexpr size_t kSize = 2 * 1000 * 1000;

  base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    void* cur = allocator->Alloc(kSize);
    CHECK_NE(cur, nullptr);
    allocator->Free(cur);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  return timer.LapsPerSecond();
}

std::unique_ptr<Allocator> CreateAllocator(AllocatorType type,
                                           bool use_alternate_bucket_dist) {
  switch (type) {
    case AllocatorType::kSystem:
      return std::make_unique<SystemAllocator>();
    case AllocatorType::kPartitionAlloc:
      return std::make_unique<PartitionAllocator>();
    case AllocatorType::kPartitionAllocWithThreadCache:
      return std::make_unique<PartitionAllocatorWithThreadCache>(
          use_alternate_bucket_dist);
  }
}

void LogResults(int thread_count,
                AllocatorType alloc_type,
                uint64_t total_laps_per_second,
                uint64_t min_laps_per_second) {
  LOG(INFO) << "RESULTSCSV: " << thread_count << ","
            << static_cast<int>(alloc_type) << "," << total_laps_per_second
            << "," << min_laps_per_second;
}

void RunTest(int thread_count,
             bool use_alternate_bucket_dist,
             AllocatorType alloc_type,
             float (*test_fn)(Allocator*),
             float (*noisy_neighbor_fn)(Allocator*),
             const char* story_base_name) {
  auto alloc = CreateAllocator(alloc_type, use_alternate_bucket_dist);

  std::unique_ptr<TestLoopThread> noisy_neighbor_thread = nullptr;
  if (noisy_neighbor_fn) {
    noisy_neighbor_thread = std::make_unique<TestLoopThread>(
        base::BindOnce(noisy_neighbor_fn, base::Unretained(alloc.get())));
  }

  std::vector<std::unique_ptr<TestLoopThread>> threads;
  for (int i = 0; i < thread_count; ++i) {
    threads.push_back(std::make_unique<TestLoopThread>(
        base::BindOnce(test_fn, base::Unretained(alloc.get()))));
  }

  uint64_t total_laps_per_second = 0;
  uint64_t min_laps_per_second = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < thread_count; ++i) {
    uint64_t laps_per_second = threads[i]->Run();
    min_laps_per_second = std::min(laps_per_second, min_laps_per_second);
    total_laps_per_second += laps_per_second;
  }

  if (noisy_neighbor_thread)
    noisy_neighbor_thread->Run();

  char const* alloc_type_str;
  switch (alloc_type) {
    case AllocatorType::kSystem:
      alloc_type_str = "System";
      break;
    case AllocatorType::kPartitionAlloc:
      alloc_type_str = "PartitionAlloc";
      break;
    case AllocatorType::kPartitionAllocWithThreadCache:
      alloc_type_str = "PartitionAllocWithThreadCache";
      break;
  }

  std::string name =
      base::StringPrintf("%s%s_%s_%d", kMetricPrefixMemoryAllocation,
                         story_base_name, alloc_type_str, thread_count);

  DisplayResults(name + "_total", total_laps_per_second);
  DisplayResults(name + "_worst", min_laps_per_second);
  LogResults(thread_count, alloc_type, total_laps_per_second,
             min_laps_per_second);
}

class PartitionAllocMemoryAllocationPerfTest
    : public testing::TestWithParam<std::tuple<int, bool, AllocatorType>> {};

// Only one partition with a thread cache: cannot use the thread cache when
// PartitionAlloc is malloc().
INSTANTIATE_TEST_SUITE_P(
    ,
    PartitionAllocMemoryAllocationPerfTest,
    ::testing::Combine(
        ::testing::Values(1, 2, 3, 4),
        ::testing::Values(false, true),
        ::testing::Values(AllocatorType::kSystem,
                          AllocatorType::kPartitionAlloc,
                          AllocatorType::kPartitionAllocWithThreadCache)));

// This test (and the other one below) allocates a large amount of memory, which
// can cause issues on Android.
#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest, SingleBucket) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), SingleBucket, nullptr,
          "SingleBucket");
}
#endif  // defined(MEMORY_CONSTRAINED)

TEST_P(PartitionAllocMemoryAllocationPerfTest, SingleBucketWithFree) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), SingleBucketWithFree, nullptr,
          "SingleBucketWithFree");
}

#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest, MultiBucket) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucket, nullptr, "MultiBucket");
}
#endif  // defined(MEMORY_CONSTRAINED)

TEST_P(PartitionAllocMemoryAllocationPerfTest, MultiBucketWithFree) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucketWithFree, nullptr,
          "MultiBucketWithFree");
}

TEST_P(PartitionAllocMemoryAllocationPerfTest, DirectMapped) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), DirectMapped, nullptr,
          "DirectMapped");
}

#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest,
       DISABLED_MultiBucketWithNoisyNeighbor) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucket, DirectMapped,
          "MultiBucketWithNoisyNeighbor");
}
#endif  // !defined(MEMORY_CONSTRAINED)

}  // namespace

}  // namespace partition_alloc::internal
