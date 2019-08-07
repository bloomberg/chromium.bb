// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/process_node_impl.h"

#include "base/process/process.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class ProcessNodeImplTest : public GraphTestHarness {};

}  // namespace

TEST_F(ProcessNodeImplTest, GetIndexingKey) {
  auto process = CreateNode<ProcessNodeImpl>();
  EXPECT_EQ(
      process->GetIndexingKey(),
      static_cast<const void*>(static_cast<const NodeBase*>(process.get())));
}

TEST_F(ProcessNodeImplTest, MeasureCPUUsage) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  process_node->SetCPUUsage(1.0);
  EXPECT_EQ(1.0, process_node->cpu_usage());
}

TEST_F(ProcessNodeImplTest, ProcessLifeCycle) {
  auto process_node = CreateNode<ProcessNodeImpl>();

  // Test the potential lifecycles of a process node.
  // First go to exited without an intervening process attached, as would happen
  // in the case the process fails to start.
  EXPECT_FALSE(process_node->process().IsValid());
  EXPECT_FALSE(process_node->exit_status());
  constexpr int32_t kExitStatus = 0xF00;
  process_node->SetProcessExitStatus(kExitStatus);
  EXPECT_TRUE(process_node->exit_status());
  EXPECT_EQ(kExitStatus, process_node->exit_status().value());

  // Next go through PID->exit status.
  const base::Process self = base::Process::Current();
  const base::Time launch_time = base::Time::Now();
  process_node->SetProcess(self.Duplicate(), launch_time);
  EXPECT_TRUE(process_node->process().IsValid());
  EXPECT_EQ(self.Pid(), process_node->process_id());
  EXPECT_EQ(launch_time, process_node->launch_time());

  // Resurrection should clear the exit status.
  EXPECT_FALSE(process_node->exit_status());

  EXPECT_EQ(0U, process_node->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta(), process_node->cumulative_cpu_usage());

  constexpr base::TimeDelta kCpuUsage = base::TimeDelta::FromMicroseconds(1);
  process_node->set_private_footprint_kb(10u);
  process_node->set_cumulative_cpu_usage(kCpuUsage);

  // Kill it again.
  // Verify that the process is cleared, but the properties stick around.
  process_node->SetProcessExitStatus(kExitStatus);
  EXPECT_FALSE(process_node->process().IsValid());
  EXPECT_EQ(self.Pid(), process_node->process_id());

  EXPECT_EQ(launch_time, process_node->launch_time());
  EXPECT_EQ(10u, process_node->private_footprint_kb());
  EXPECT_EQ(kCpuUsage, process_node->cumulative_cpu_usage());

  // Resurrect again and verify the launch time and measurements
  // are cleared.
  const base::Time launch2_time = launch_time + base::TimeDelta::FromSeconds(1);
  process_node->SetProcess(self.Duplicate(), launch2_time);

  EXPECT_EQ(launch2_time, process_node->launch_time());
  EXPECT_EQ(0U, process_node->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta(), process_node->cumulative_cpu_usage());
}

TEST_F(ProcessNodeImplTest, GetPageNodeIfExclusive) {
  {
    MockSinglePageInSingleProcessGraph g(graph());
    EXPECT_EQ(g.page.get(), g.process.get()->GetPageNodeIfExclusive());
  }

  {
    MockSinglePageWithMultipleProcessesGraph g(graph());
    EXPECT_EQ(g.page.get(), g.process.get()->GetPageNodeIfExclusive());
  }

  {
    MockMultiplePagesInSingleProcessGraph g(graph());
    EXPECT_FALSE(g.process.get()->GetPageNodeIfExclusive());
  }

  {
    MockMultiplePagesWithMultipleProcessesGraph g(graph());
    EXPECT_FALSE(g.process.get()->GetPageNodeIfExclusive());
    EXPECT_EQ(g.other_page.get(),
              g.other_process.get()->GetPageNodeIfExclusive());
  }
}

}  // namespace performance_manager
