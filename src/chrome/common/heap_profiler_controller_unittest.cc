// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

TEST(HeapProfilerControllerTest, EmptyProfileIsNotEmitted) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  HeapProfilerController controller;
  metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
      base::BindLambdaForTesting(
          [](base::TimeTicks time, metrics::SampledProfile profile) {
            ADD_FAILURE();
          }));
  controller.SetTaskRunnerForTest(task_runner);
  controller.Start();

  task_runner->FastForwardBy(base::TimeDelta::FromDays(365));
}

// Sampling profiler is not capable of unwinding stack on Android under tests.
#if !defined(OS_ANDROID)
TEST(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
  constexpr size_t kAllocationSize = 42 * 1024;
  constexpr int kSnapshotsToCollect = 3;

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  auto controller = std::make_unique<HeapProfilerController>();
  int profile_count = 0;

  auto check_profile = [&](base::TimeTicks time,
                           metrics::SampledProfile profile) {
    EXPECT_EQ(metrics::SampledProfile::PERIODIC_HEAP_COLLECTION,
              profile.trigger_event());
    EXPECT_LT(0, profile.call_stack_profile().stack_sample_size());

    bool found = false;
    for (const metrics::CallStackProfile::StackSample& sample :
         profile.call_stack_profile().stack_sample()) {
      if (sample.has_weight() &&
          static_cast<size_t>(sample.weight()) >= kAllocationSize) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);

    if (++profile_count == kSnapshotsToCollect)
      controller.reset();
  };

  base::SamplingHeapProfiler::Init();
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(1024);
  profiler->Start();

  metrics::CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
      base::BindLambdaForTesting(check_profile));
  controller->SetTaskRunnerForTest(task_runner);
  controller->Start();

  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x7331), kAllocationSize,
                       base::PoissonAllocationSampler::kMalloc, nullptr);

  task_runner->FastForwardUntilNoTasksRemain();
  EXPECT_LE(kSnapshotsToCollect, profile_count);
}
#endif
