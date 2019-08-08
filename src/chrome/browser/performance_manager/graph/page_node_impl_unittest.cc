// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/page_node_impl.h"

#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class PageNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { PerformanceManagerClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(PageNodeImplTest, AddFrameBasic) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>(nullptr);
  auto parent_frame = CreateNode<FrameNodeImpl>(process_node.get(),
                                                page_node.get(), nullptr, 0);
  auto child1_frame = CreateNode<FrameNodeImpl>(
      process_node.get(), page_node.get(), parent_frame.get(), 1);
  auto child2_frame = CreateNode<FrameNodeImpl>(
      process_node.get(), page_node.get(), parent_frame.get(), 2);

  // Validate that all frames are tallied to the page.
  EXPECT_EQ(3u, page_node->GetFrameNodes().size());
}

TEST_F(PageNodeImplTest, RemoveFrame) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>(nullptr);
  auto frame_node = CreateNode<FrameNodeImpl>(process_node.get(),
                                              page_node.get(), nullptr, 0);

  // Ensure correct page-frame relationship has been established.
  EXPECT_EQ(1u, page_node->GetFrameNodes().size());
  EXPECT_TRUE(
      base::ContainsValue(page_node->GetFrameNodes(), frame_node.get()));
  EXPECT_EQ(page_node.get(), frame_node->page_node());

  frame_node.reset();

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, page_node->GetFrameNodes().size());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(40, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(20, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(70, mock_graph.page->GetCPUUsage());
}

TEST_F(PageNodeImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  mock_graph.process->SetCPUUsage(40);
  mock_graph.other_process->SetCPUUsage(30);
  EXPECT_EQ(20, mock_graph.page->GetCPUUsage());
  EXPECT_EQ(50, mock_graph.other_page->GetCPUUsage());
}

TEST_F(PageNodeImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  mock_graph.page->SetIsVisible(true);
  EXPECT_TRUE(mock_graph.page->is_visible());
  AdvanceClock(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            mock_graph.page->TimeSinceLastVisibilityChange());

  mock_graph.page->SetIsVisible(false);
  AdvanceClock(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            mock_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(mock_graph.page->is_visible());
}

TEST_F(PageNodeImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(mock_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  GURL url("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(
      PerformanceManagerClock::NowTicks(), 10u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(10u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            mock_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  url = GURL("http://www.example.org/bobcat");
  mock_graph.page->OnMainFrameNavigationCommitted(
      PerformanceManagerClock::NowTicks(), 20u, url);
  EXPECT_EQ(url, mock_graph.page->main_frame_url());
  EXPECT_EQ(20u, mock_graph.page->navigation_id());
  AdvanceClock(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            mock_graph.page->TimeSinceLastNavigation());
}

TEST_F(PageNodeImplTest, IsLoading) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page_node = mock_graph.page.get();

  // This should be initialized to false.
  EXPECT_FALSE(page_node->is_loading());

  // Set to false and the property should stay false.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());

  // Set to true and the property should read true.
  page_node->SetIsLoading(true);
  EXPECT_TRUE(page_node->is_loading());

  // Set to false and the property should read false again.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
}

namespace {

const size_t kInterventionCount =
    static_cast<size_t>(
        resource_coordinator::mojom::PolicyControlledIntervention::kMaxValue) +
    1;

void ExpectRawInterventionPolicy(
    resource_coordinator::mojom::InterventionPolicy policy,
    const PageNodeImpl* page_node) {
  for (size_t i = 0; i < kInterventionCount; ++i) {
    EXPECT_EQ(
        policy,
        page_node->GetRawInterventionPolicyForTesting(
            static_cast<
                resource_coordinator::mojom::PolicyControlledIntervention>(i)));
  }
}

void ExpectInterventionPolicy(
    resource_coordinator::mojom::InterventionPolicy policy,
    PageNodeImpl* page_node) {
  for (size_t i = 0; i < kInterventionCount; ++i) {
    EXPECT_EQ(
        policy,
        page_node->GetInterventionPolicy(
            static_cast<
                resource_coordinator::mojom::PolicyControlledIntervention>(i)));
  }
}

void ExpectInitialInterventionPolicyAggregationWorks(
    Graph* mock_graph,
    resource_coordinator::mojom::InterventionPolicy f0_policy,
    resource_coordinator::mojom::InterventionPolicy f1_policy,
    resource_coordinator::mojom::InterventionPolicy f0_policy_aggregated,
    resource_coordinator::mojom::InterventionPolicy f0f1_policy_aggregated) {
  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph, nullptr);

  // Check the initial values before any frames are added.
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());

  // Create an initial frame.
  TestNodeWrapper<FrameNodeImpl> f0 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), nullptr, 0);
  // Add a frame and expect the values to be invalidated. Reaggregate and
  // ensure the appropriate value results.
  f0->SetAllInterventionPoliciesForTesting(f0_policy);
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0_policy_aggregated, page.get());

  TestNodeWrapper<FrameNodeImpl> f1 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), f0.get(), 1);
  // Do it again. This time the raw values should be the same as the
  // aggregated values above.
  f1->SetAllInterventionPoliciesForTesting(f1_policy);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0f1_policy_aggregated, page.get());

  // Remove a frame and expect the values to be invalidated again.
  f1.reset();
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(f0_policy_aggregated, page.get());
}

}  // namespace

TEST_F(PageNodeImplTest, InitialInterventionPolicy) {
  auto* mock_graph = graph();

  // Tests all possible transitions where the frame CU has its policy values
  // set before being attached to the page CU. This affectively tests the
  // aggregation logic in isolation.

  // Default x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kDefault /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  // OptIn x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptIn /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  // OptOut x [Default, OptIn, OptOut]

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kDefault /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptIn /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);

  ExpectInitialInterventionPolicyAggregationWorks(
      mock_graph,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f0_policy */,
      resource_coordinator::mojom::InterventionPolicy::kOptOut /* f1_policy */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0_policy_aggregated */,
      resource_coordinator::mojom::InterventionPolicy::
          kOptOut /* f0f1_policy_aggregated */);
}

TEST_F(PageNodeImplTest, IncrementalInterventionPolicy) {
  auto* mock_graph = graph();

  TestNodeWrapper<ProcessNodeImpl> process =
      TestNodeWrapper<ProcessNodeImpl>::Create(mock_graph);
  TestNodeWrapper<PageNodeImpl> page =
      TestNodeWrapper<PageNodeImpl>::Create(mock_graph, nullptr);

  // Create two frames and immediately attach them to the page.
  TestNodeWrapper<FrameNodeImpl> f0 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), nullptr, 0);
  TestNodeWrapper<FrameNodeImpl> f1 = TestNodeWrapper<FrameNodeImpl>::Create(
      mock_graph, process.get(), page.get(), f0.get(), 1);
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());
  EXPECT_EQ(0u, page->GetInterventionPolicyFramesReportedForTesting());

  // Set the policies on the first frame. This should be observed by the page
  // CU, but aggregation should still not be possible.
  f0->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  EXPECT_EQ(1u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());

  // Now set the policy on the second frame. This should be observed and an
  // aggregated page policy value should now be available.
  f1->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kDefault);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kDefault, page.get());

  // Change the policy value on a frame and expect a new aggregation to be
  // required.
  f1->SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy::kOptIn);
  EXPECT_EQ(2u, page->GetInterventionPolicyFramesReportedForTesting());
  ExpectRawInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kUnknown, page.get());
  ExpectInterventionPolicy(
      resource_coordinator::mojom::InterventionPolicy::kOptIn, page.get());
}

}  // namespace performance_manager
