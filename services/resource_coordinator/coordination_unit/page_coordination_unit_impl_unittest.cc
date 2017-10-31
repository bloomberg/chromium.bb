// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class PageCoordinationUnitImplTest : public CoordinationUnitTestHarness {};

}  // namespace

TEST_F(PageCoordinationUnitImplTest, AddFrameBasic) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame3_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  page_cu->AddFrame(frame1_cu->id());
  page_cu->AddFrame(frame2_cu->id());
  page_cu->AddFrame(frame3_cu->id());
  EXPECT_EQ(3u, page_cu->frame_coordination_units_for_testing().size());
}

TEST_F(PageCoordinationUnitImplTest, AddReduplicativeFrame) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame1_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();
  auto frame2_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  page_cu->AddFrame(frame1_cu->id());
  page_cu->AddFrame(frame2_cu->id());
  page_cu->AddFrame(frame1_cu->id());
  EXPECT_EQ(2u, page_cu->frame_coordination_units_for_testing().size());
}

TEST_F(PageCoordinationUnitImplTest, RemoveFrame) {
  auto page_cu = CreateCoordinationUnit<PageCoordinationUnitImpl>();
  auto frame_cu = CreateCoordinationUnit<FrameCoordinationUnitImpl>();

  // Parent-child relationships have not been established yet.
  EXPECT_EQ(0u, page_cu->frame_coordination_units_for_testing().size());
  EXPECT_FALSE(frame_cu->GetPageCoordinationUnit());

  page_cu->AddFrame(frame_cu->id());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, page_cu->frame_coordination_units_for_testing().size());
  EXPECT_EQ(1u, page_cu->frame_coordination_units_for_testing().count(
                    frame_cu.get()));
  EXPECT_EQ(page_cu.get(), frame_cu->GetPageCoordinationUnit());

  page_cu->RemoveFrame(frame_cu->id());

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, page_cu->frame_coordination_units_for_testing().size());
  EXPECT_FALSE(frame_cu->GetPageCoordinationUnit());
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetCPUUsage(40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(40, cpu_usage / 1000);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetCPUUsage(40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage / 1000);
  EXPECT_TRUE(cu_graph.other_page->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &cpu_usage));
  EXPECT_EQ(20, cpu_usage / 1000);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetCPUUsage(40);
  cu_graph.other_process->SetCPUUsage(30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(70, cpu_usage / 1000);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageCPUUsageForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetCPUUsage(40);
  cu_graph.other_process->SetCPUUsage(30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.page->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage / 1000);
  EXPECT_TRUE(cu_graph.other_page->GetProperty(mojom::PropertyType::kCPUUsage,
                                               &cpu_usage));
  EXPECT_EQ(50, cpu_usage / 1000);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));

  int64_t eqt;
  ASSERT_TRUE(cu_graph.page->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageCoordinationUnitImplTest,
       CalculatePageEQTForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));

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
  cu_graph.page->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));

  cu_graph.page->SetVisibility(true);
  EXPECT_TRUE(cu_graph.page->IsVisible());
  clock->Advance(base::TimeDelta::FromSeconds(42));
  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            cu_graph.page->TimeSinceLastVisibilityChange());

  cu_graph.page->SetVisibility(false);
  clock->Advance(base::TimeDelta::FromSeconds(23));
  EXPECT_EQ(base::TimeDelta::FromSeconds(23),
            cu_graph.page->TimeSinceLastVisibilityChange());
  EXPECT_FALSE(cu_graph.page->IsVisible());
  // The clock is owned and destructed by the page cu.
  clock = nullptr;
}

TEST_F(PageCoordinationUnitImplTest, TimeSinceLastNavigation) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  // Sets a valid starting time.
  clock->SetNowTicks(base::TimeTicks::Now());
  cu_graph.page->SetClockForTest(
      std::unique_ptr<base::SimpleTestTickClock>(clock));
  // Before any commit events, timedelta should be 0.
  EXPECT_TRUE(cu_graph.page->TimeSinceLastNavigation().is_zero());

  // 1st navigation.
  cu_graph.page->OnMainFrameNavigationCommitted();
  clock->Advance(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta::FromSeconds(11),
            cu_graph.page->TimeSinceLastNavigation());

  // 2nd navigation.
  cu_graph.page->OnMainFrameNavigationCommitted();
  clock->Advance(base::TimeDelta::FromSeconds(17));
  EXPECT_EQ(base::TimeDelta::FromSeconds(17),
            cu_graph.page->TimeSinceLastNavigation());
  // The clock is owned and destructed by the page cu.
  clock = nullptr;
}

}  // namespace resource_coordinator
