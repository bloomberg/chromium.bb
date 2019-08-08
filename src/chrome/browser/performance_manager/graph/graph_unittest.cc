// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/graph.h"

#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

TEST(GraphTest, FindOrCreateSystemNode) {
  Graph graph;

  SystemNodeImpl* system_node = graph.FindOrCreateSystemNode();

  // A second request should return the same instance.
  EXPECT_EQ(system_node, graph.FindOrCreateSystemNode());
}

TEST(GraphTest, GetProcessNodeByPid) {
  Graph graph;

  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(&graph);
  EXPECT_EQ(base::kNullProcessId, process->process_id());
  EXPECT_FALSE(process->process().IsValid());

  const base::Process self = base::Process::Current();

  EXPECT_EQ(nullptr, graph.GetProcessNodeByPid(self.Pid()));
  process->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_TRUE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph.GetProcessNodeByPid(self.Pid()));

  // Validate that an exited process isn't removed (yet).
  process->SetProcessExitStatus(0xCAFE);
  EXPECT_FALSE(process->process().IsValid());
  EXPECT_EQ(self.Pid(), process->process_id());
  EXPECT_EQ(process.get(), graph.GetProcessNodeByPid(self.Pid()));

  process.reset();

  EXPECT_EQ(nullptr, graph.GetProcessNodeByPid(self.Pid()));
}

TEST(GraphTest, PIDReuse) {
  // This test emulates what happens on Windows under aggressive PID reuse,
  // where a process termination notification can be delayed until after the
  // PID has been reused for a new process.
  Graph graph;

  static base::Process self = base::Process::Current();

  TestNodeWrapper<ProcessNodeImpl> process1 =
      TestNodeWrapper<ProcessNodeImpl>::Create(&graph);
  TestNodeWrapper<ProcessNodeImpl> process2 =
      TestNodeWrapper<ProcessNodeImpl>::Create(&graph);

  process1->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process1.get(), graph.GetProcessNodeByPid(self.Pid()));

  // First process exits, but hasn't been deleted yet.
  process1->SetProcessExitStatus(0xCAFE);
  EXPECT_EQ(process1.get(), graph.GetProcessNodeByPid(self.Pid()));

  // The second registration for the same PID should override the first one.
  process2->SetProcess(self.Duplicate(), base::Time::Now());
  EXPECT_EQ(process2.get(), graph.GetProcessNodeByPid(self.Pid()));

  // The destruction of the first process CU shouldn't clear the PID
  // registration.
  process1.reset();
  EXPECT_EQ(process2.get(), graph.GetProcessNodeByPid(self.Pid()));
}

TEST(GraphTest, GetAllCUsByType) {
  Graph graph;
  MockMultiplePagesInSingleProcessGraph mock_graph(&graph);

  std::vector<ProcessNodeImpl*> processes = graph.GetAllProcessNodes();
  ASSERT_EQ(1u, processes.size());
  EXPECT_NE(nullptr, processes[0]);

  std::vector<FrameNodeImpl*> frames = graph.GetAllFrameNodes();
  ASSERT_EQ(2u, frames.size());
  EXPECT_NE(nullptr, frames[0]);
  EXPECT_NE(nullptr, frames[1]);

  std::vector<PageNodeImpl*> pages = graph.GetAllPageNodes();
  ASSERT_EQ(2u, pages.size());
  EXPECT_NE(nullptr, pages[0]);
  EXPECT_NE(nullptr, pages[1]);
}

}  // namespace performance_manager
