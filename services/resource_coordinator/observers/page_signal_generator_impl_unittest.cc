// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/page_signal_generator_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class MockPageSignalGeneratorImpl : public PageSignalGeneratorImpl {
 public:
  // Overridden from PageSignalGeneratorImpl.
  void OnProcessPropertyChanged(const ProcessCoordinationUnitImpl* process_cu,
                                const mojom::PropertyType property_type,
                                int64_t value) override {
    if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class PageSignalGeneratorImplTest : public CoordinationUnitTestHarness {
 protected:
  MockPageSignalGeneratorImpl* page_signal_generator() {
    return &page_signal_generator_;
  }

 private:
  MockPageSignalGeneratorImpl page_signal_generator_;
};

TEST_F(PageSignalGeneratorImplTest,
       CalculatePageEQTForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;
  cu_graph.process->AddObserver(page_signal_generator());

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));
  cu_graph.other_process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(10));

  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the page.
  EXPECT_EQ(1u, page_signal_generator()->eqt_change_count());
  int64_t eqt;
  EXPECT_TRUE(cu_graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageSignalGeneratorImplTest, IsLoading) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  auto* page_cu = cu_graph.page.get();
  auto* psg = page_signal_generator();
  // The observer relationship isn't required for testing IsLoading.

  // The loading property hasn't yet been set. Then IsLoading should return
  // false as the default value.
  EXPECT_FALSE(psg->IsLoading(page_cu));

  // Once the loading property has been set it should return that value.
  page_cu->SetIsLoading(false);
  EXPECT_FALSE(psg->IsLoading(page_cu));
  page_cu->SetIsLoading(true);
  EXPECT_TRUE(psg->IsLoading(page_cu));
  page_cu->SetIsLoading(false);
  EXPECT_FALSE(psg->IsLoading(page_cu));
}

TEST_F(PageSignalGeneratorImplTest, IsIdling) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  auto* frame_cu = cu_graph.frame.get();
  auto* page_cu = cu_graph.page.get();
  auto* proc_cu = cu_graph.process.get();
  auto* psg = page_signal_generator();
  // The observer relationship isn't required for testing IsIdling.

  // Neither of the idling properties are set, so IsIdling should return false.
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // Should still return false after main thread task is low.
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // Should return true when network is idle.
  frame_cu->SetNetworkAlmostIdle(true);
  EXPECT_TRUE(psg->IsIdling(page_cu));

  // Should toggle with main thread task low.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  EXPECT_TRUE(psg->IsIdling(page_cu));

  // Should return false when network is no longer idle.
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));

  // And should stay false if main thread task also goes low again.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_FALSE(psg->IsIdling(page_cu));
}

TEST_F(PageSignalGeneratorImplTest, PageDataCorrectlyManaged) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  auto* page_cu = cu_graph.page.get();
  auto* psg = page_signal_generator();
  // The observer relationship isn't required for testing GetPageData.

  EXPECT_EQ(0u, psg->page_data_.count(page_cu));
  psg->OnCoordinationUnitCreated(page_cu);
  EXPECT_EQ(1u, psg->page_data_.count(page_cu));
  EXPECT_TRUE(psg->GetPageData(page_cu));
  psg->OnBeforeCoordinationUnitDestroyed(page_cu);
  EXPECT_EQ(0u, psg->page_data_.count(page_cu));
}

TEST_F(PageSignalGeneratorImplTest, PageAlmostIdleTransitions) {
  ResourceCoordinatorClock::SetClockForTesting(
      std::make_unique<base::SimpleTestTickClock>());
  auto* test_clock = static_cast<base::SimpleTestTickClock*>(
      ResourceCoordinatorClock::GetClockForTesting());
  test_clock->Advance(base::TimeDelta::FromSeconds(1));

  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  auto* frame_cu = cu_graph.frame.get();
  auto* page_cu = cu_graph.page.get();
  auto* proc_cu = cu_graph.process.get();
  auto* psg = page_signal_generator();

  // Set up observers, as the PAI logic depends on them.
  frame_cu->AddObserver(psg);
  page_cu->AddObserver(psg);
  proc_cu->AddObserver(psg);

  // Ensure the page_cu creation is witnessed and get the associated
  // page data for testing, then bind the timer to the test task runner.
  psg->OnCoordinationUnitCreated(page_cu);
  PageSignalGeneratorImpl::PageData* page_data = psg->GetPageData(page_cu);
  page_data->idling_timer.SetTaskRunner(task_env().GetMainThreadTaskRunner());

  // Aliasing these here makes this unittest much more legible.
  using LIS = PageSignalGeneratorImpl::LoadIdleState;

  // Initially the page should be in a loading not started state.
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // The state should not transition when a not loading state is explicitly
  // set.
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // The state should transition to loading when loading starts.
  page_cu->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoading, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Mark the page as idling. It should transition from kLoading directly
  // to kLoadedAndIdling after this.
  frame_cu->SetNetworkAlmostIdle(true);
  proc_cu->SetMainThreadTaskLoadIsLow(true);
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state);
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  // Indicate loading is happening again. This should be ignored.
  page_cu->SetIsLoading(true);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state);
  EXPECT_TRUE(page_data->idling_timer.IsRunning());
  page_cu->SetIsLoading(false);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state);
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  // Go back to not idling. We should transition back to kLoadedNotIdling.
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_EQ(LIS::kLoadedNotIdling, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Go back to idling.
  frame_cu->SetNetworkAlmostIdle(true);
  EXPECT_EQ(LIS::kLoadedAndIdling, page_data->load_idle_state);
  EXPECT_TRUE(page_data->idling_timer.IsRunning());

  // Let the timer evaluate. The final state transition should occur.
  // TODO(chrisha): Expose the tick clock from the ScopedTaskEnvironment, and
  // use that clock in the ResourceCoordinatorClock. Beats having two separate
  // fake clocks that need to be manually kept in sync.
  test_clock->Advance(PageSignalGeneratorImpl::kLoadedAndIdlingTimeout);
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Firing other signals should not change the state at all.
  proc_cu->SetMainThreadTaskLoadIsLow(false);
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());
  frame_cu->SetNetworkAlmostIdle(false);
  EXPECT_EQ(LIS::kLoadedAndIdle, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());

  // Post a navigation. The state should reset.
  page_cu->OnMainFrameNavigationCommitted();
  EXPECT_EQ(LIS::kLoadingNotStarted, page_data->load_idle_state);
  EXPECT_FALSE(page_data->idling_timer.IsRunning());
}

}  // namespace resource_coordinator
