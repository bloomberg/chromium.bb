// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class PageCoordinationUnitImplTest : public CoordinationUnitTestHarness {};

}  // namespace

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(40, cpu_usage);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
  EXPECT_TRUE(cu_graph.other_page->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage, 30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(70, cpu_usage);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage, 30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
  EXPECT_TRUE(cu_graph.other_page->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &cpu_usage));
  EXPECT_EQ(50, cpu_usage);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);

  int64_t eqt;
  ASSERT_TRUE(cu_graph.page->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);

  int64_t eqt;
  ASSERT_TRUE(cu_graph.page->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
  ASSERT_TRUE(cu_graph.other_page->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageCoordinationUnitImplTest, TimeSinceLastVisibilityChange) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  PageCoordinationUnitImpl* page_cu =
      CoordinationUnitBase::ToPageCoordinationUnit(cu_graph.page.get());
  page_cu->SetClockForTest(std::unique_ptr<base::SimpleTestTickClock>(clock));

  cu_graph.page->SetProperty(mojom::PropertyType::kVisible, true);
  EXPECT_TRUE(page_cu->IsVisible());
  clock->Advance(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            page_cu->TimeSinceLastVisibilityChange());

  cu_graph.page->SetProperty(mojom::PropertyType::kVisible, false);
  clock->Advance(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            page_cu->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(page_cu->IsVisible());
  // The clock is owned and destructed by the page cu.
  clock = nullptr;
}

TEST_F(PageCoordinationUnitImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  // Sets a valid starting time.
  clock->SetNowTicks(base::TimeTicks::Now());
  PageCoordinationUnitImpl* page_cu =
      CoordinationUnitBase::ToPageCoordinationUnit(cu_graph.page.get());
  page_cu->SetClockForTest(std::unique_ptr<base::SimpleTestTickClock>(clock));
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(page_cu->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  cu_graph.page->SendEvent(mojom::Event::kNavigationCommitted);
  clock->Advance(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            page_cu->TimeSinceLastNavigation());

  // 2nd navigation.
  cu_graph.page->SendEvent(mojom::Event::kNavigationCommitted);
  clock->Advance(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            page_cu->TimeSinceLastNavigation());
  // The clock is owned and destructed by the page cu.
  clock = nullptr;
}

}  // namespace resource_coordinator
