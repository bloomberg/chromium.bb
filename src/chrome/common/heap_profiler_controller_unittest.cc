// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include "base/command_line.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/metrics/legacy_call_stack_profile_builder.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

TEST(HeapProfilerControllerTest, ProfileCollectionsScheduler) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kSamplingHeapProfiler, "1");

  int profiles_collected = 0;
  metrics::LegacyCallStackProfileBuilder::SetBrowserProcessReceiverCallback(
      base::BindLambdaForTesting(
          [&](base::TimeTicks time, metrics::SampledProfile profile) {
            EXPECT_EQ(metrics::SampledProfile::PERIODIC_HEAP_COLLECTION,
                      profile.trigger_event());
            EXPECT_LE(1, profile.call_stack_profile().deprecated_sample_size());
            ++profiles_collected;
          }));
  base::PoissonAllocationSampler::Init();
  HeapProfilerController controller;
  controller.SetTaskRunnerForTest(task_runner.get());
  controller.StartIfEnabled();
  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->RecordAlloc(reinterpret_cast<void*>(0x1337), 42000,
                       base::PoissonAllocationSampler::kMalloc, nullptr);
  do {
    task_runner->FastForwardBy(base::TimeDelta::FromHours(1));
  } while (profiles_collected < 2);
}
