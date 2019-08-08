// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/mock_graphs.h"

#include <string>

#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"

namespace performance_manager {

TestProcessNodeImpl::TestProcessNodeImpl(GraphImpl* graph)
    : ProcessNodeImpl(graph) {}

void TestProcessNodeImpl::SetProcessWithPid(base::ProcessId pid,
                                            base::Process process,
                                            base::Time launch_time) {
  SetProcessImpl(std::move(process), pid, launch_time);
}

MockSinglePageInSingleProcessGraph::MockSinglePageInSingleProcessGraph(
    GraphImpl* graph)
    : system(TestNodeWrapper<SystemNodeImpl>::Create(graph)),
      process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                   process.get(),
                                                   page.get())) {
  frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  process->SetProcessWithPid(1, base::Process::Current(), base::Time::Now());
}

MockSinglePageInSingleProcessGraph::~MockSinglePageInSingleProcessGraph() {
  // Make sure frame nodes are torn down before pages.
  frame.reset();
  page.reset();
}

MockMultiplePagesInSingleProcessGraph::MockMultiplePagesInSingleProcessGraph(
    GraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_page(TestNodeWrapper<PageNodeImpl>::Create(graph)),
      other_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         process.get(),
                                                         other_page.get(),
                                                         nullptr,
                                                         1)) {
  other_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockMultiplePagesInSingleProcessGraph::
    ~MockMultiplePagesInSingleProcessGraph() {
  other_frame.reset();
  other_page.reset();
}

MockSinglePageWithMultipleProcessesGraph::
    MockSinglePageWithMultipleProcessesGraph(GraphImpl* graph)
    : MockSinglePageInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         other_process.get(),
                                                         page.get(),
                                                         frame.get(),
                                                         2)) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   base::Time::Now());
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockSinglePageWithMultipleProcessesGraph::
    ~MockSinglePageWithMultipleProcessesGraph() = default;

MockMultiplePagesWithMultipleProcessesGraph::
    MockMultiplePagesWithMultipleProcessesGraph(GraphImpl* graph)
    : MockMultiplePagesInSingleProcessGraph(graph),
      other_process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph)),
      child_frame(TestNodeWrapper<FrameNodeImpl>::Create(graph,
                                                         other_process.get(),
                                                         other_page.get(),
                                                         other_frame.get(),
                                                         3)) {
  other_process->SetProcessWithPid(2, base::Process::Current(),
                                   base::Time::Now());
  child_frame->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
}

MockMultiplePagesWithMultipleProcessesGraph::
    ~MockMultiplePagesWithMultipleProcessesGraph() = default;

}  // namespace performance_manager
