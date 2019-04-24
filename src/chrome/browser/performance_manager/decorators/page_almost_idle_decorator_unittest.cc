// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/common/page_almost_idle_data.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class LenientMockGraphObserver : public GraphObserver {
 public:
  LenientMockGraphObserver() = default;
  ~LenientMockGraphObserver() override = default;

  virtual bool ShouldObserve(const NodeBase* node) {
    return node->id().type == PageNodeImpl::Type();
  }

  MOCK_METHOD1(OnPageAlmostIdleChanged, void(PageNodeImpl*));

  void ExpectOnPageAlmostIdleChanged(PageNodeImpl* page_node,
                                     bool page_almost_idle) {
    EXPECT_CALL(*this, OnPageAlmostIdleChanged(page_node))
        .WillOnce(::testing::InvokeWithoutArgs([page_node, page_almost_idle]() {
          EXPECT_EQ(page_almost_idle, page_node->page_almost_idle());
        }));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LenientMockGraphObserver);
};

using MockGraphObserver = ::testing::StrictMock<LenientMockGraphObserver>;

}  // namespace

// A simple wrapper that exposes the internals of PageAlmostIdleData.
class TestPageAlmostIdleData : public PageAlmostIdleData {
 public:
  using PageAlmostIdleData::idling_timer_;
  using PageAlmostIdleData::load_idle_state_;
  using PageAlmostIdleData::LoadIdleState;
};
static_assert(sizeof(TestPageAlmostIdleData) == sizeof(PageAlmostIdleData) &&
                  !std::is_abstract<PageAlmostIdleData>::value &&
                  !std::is_polymorphic<PageAlmostIdleData>::value,
              "reinterpret_cast might not be safe, rework this code!");

class PageAlmostIdleDecoratorTestHelper {
 public:
  static const base::TimeDelta kLoadedAndIdlingTimeout;
  static const base::TimeDelta kWaitingForIdleTimeout;

  static TestPageAlmostIdleData* GetOrCreateData(PageNodeImpl* page_node) {
    return reinterpret_cast<TestPageAlmostIdleData*>(
        PageAlmostIdleDecorator::GetOrCreateData(page_node));
  }

  static TestPageAlmostIdleData* GetData(PageNodeImpl* page_node) {
    return reinterpret_cast<TestPageAlmostIdleData*>(
        PageAlmostIdleDecorator::GetData(page_node));
  }

  static bool IsIdling(const PageNodeImpl* page_node) {
    return PageAlmostIdleDecorator::IsIdling(page_node);
  }
};

// static
const base::TimeDelta
    PageAlmostIdleDecoratorTestHelper::kLoadedAndIdlingTimeout =
        PageAlmostIdleDecorator::kLoadedAndIdlingTimeout;
// static
const base::TimeDelta
    PageAlmostIdleDecoratorTestHelper::kWaitingForIdleTimeout =
        PageAlmostIdleDecorator::kWaitingForIdleTimeout;

class PageAlmostIdleDecoratorTest : public GraphTestHarness {
 protected:
  // Aliasing these here makes this unittest much more legible.
  using LIS = TestPageAlmostIdleData::LoadIdleState;

  PageAlmostIdleDecoratorTest() = default;
  ~PageAlmostIdleDecoratorTest() override = default;

  void SetUp() override {
    auto paid = std::make_unique<PageAlmostIdleDecorator>();
    paid_ = paid.get();
    coordination_unit_graph()->RegisterObserver(std::move(paid));
  }
  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

  void TestPageAlmostIdleTransitions(bool timeout);

  PageAlmostIdleDecorator* paid_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleDecoratorTest);
};

void PageAlmostIdleDecoratorTest::TestPageAlmostIdleTransitions(bool timeout) {
  ResourceCoordinatorClock::SetClockForTesting(task_env().GetMockTickClock());
  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  MockSinglePageInSingleProcessGraph graph(coordination_unit_graph());
  auto* frame_node = graph.frame.get();
  auto* page_node = graph.page.get();
  auto* proc_node = graph.process.get();
  auto* page_data =
      PageAlmostIdleDecoratorTestHelper::GetOrCreateData(page_node);

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should not transition when a not loading state is explicitly
  // set.
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // The state should transition to loading when loading starts.
  page_node->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoading, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());

  // Mark the page as idling. It should transition from kLoading directly
  // to kLoadedAndIdling after this.
  frame_node->SetNetworkAlmostIdle(true);
  proc_node->SetMainThreadTaskLoadIsLow(true);
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Indicate loading is happening again. This should be ignored.
  page_node->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());
  page_node->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling, and
  // a timer should still be running.
  frame_node->SetNetworkAlmostIdle(false);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->load_idle_state_);
  EXPECT_TRUE(page_data->idling_timer_.IsRunning());

  base::TimeTicks start = ResourceCoordinatorClock::NowTicks();
  if (timeout) {
    // Let the timeout run down. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = ResourceCoordinatorClock::NowTicks();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(PageAlmostIdleDecoratorTestHelper::kLoadedAndIdlingTimeout,
              elapsed);
    EXPECT_LE(PageAlmostIdleDecoratorTestHelper::kWaitingForIdleTimeout,
              elapsed);
    EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::GetData(page_node));
  } else {
    // Go back to idling.
    frame_node->SetNetworkAlmostIdle(true);
    EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state_);
    EXPECT_TRUE(page_data->idling_timer_.IsRunning());

    // Let the idle timer evaluate. The final state transition should occur.
    task_env().FastForwardUntilNoTasksRemain();
    base::TimeTicks end = ResourceCoordinatorClock::NowTicks();
    base::TimeDelta elapsed = end - start;
    EXPECT_LE(PageAlmostIdleDecoratorTestHelper::kLoadedAndIdlingTimeout,
              elapsed);
    EXPECT_GT(PageAlmostIdleDecoratorTestHelper::kWaitingForIdleTimeout,
              elapsed);
    EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::GetData(page_node));
  }

  // Firing other signals should not change the state at all.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::GetData(page_node));
  frame_node->SetNetworkAlmostIdle(false);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::GetData(page_node));

  // Post a navigation. The state should reset.
  page_node->OnMainFrameNavigationCommitted(
      ResourceCoordinatorClock::NowTicks(), 1, "https://www.example.org");
  page_data = PageAlmostIdleDecoratorTestHelper::GetData(page_node);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state_);
  EXPECT_FALSE(page_data->idling_timer_.IsRunning());
}

TEST_F(PageAlmostIdleDecoratorTest, TestTransitionsNoTimeout) {
  TestPageAlmostIdleTransitions(false);
}

TEST_F(PageAlmostIdleDecoratorTest, TestTransitionsWithTimeout) {
  TestPageAlmostIdleTransitions(true);
}

TEST_F(PageAlmostIdleDecoratorTest, IsLoading) {
  MockSinglePageInSingleProcessGraph cu_graph(coordination_unit_graph());
  auto* page_node = cu_graph.page.get();

  // The loading property hasn't yet been set. Then IsLoading should return
  // false as the default value.
  EXPECT_FALSE(page_node->is_loading());

  // Once the loading property has been set it should return that value.
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
  page_node->SetIsLoading(true);
  EXPECT_TRUE(page_node->is_loading());
  page_node->SetIsLoading(false);
  EXPECT_FALSE(page_node->is_loading());
}

TEST_F(PageAlmostIdleDecoratorTest, IsIdling) {
  MockSinglePageInSingleProcessGraph cu_graph(coordination_unit_graph());
  auto* frame_node = cu_graph.frame.get();
  auto* page_node = cu_graph.page.get();
  auto* proc_node = cu_graph.process.get();

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));

  // Should still return false after main thread task is low.
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));

  // Should return true when network is idle.
  frame_node->SetNetworkAlmostIdle(true);
  EXPECT_TRUE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));

  // Should toggle with main thread task low.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));
  proc_node->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));

  // Should return false when network is no longer idle.
  frame_node->SetNetworkAlmostIdle(false);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));

  // And should stay false if main thread task also goes low again.
  proc_node->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(PageAlmostIdleDecoratorTestHelper::IsIdling(page_node));
}

}  // namespace performance_manager
