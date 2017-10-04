// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
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

}  // namespace resource_coordinator
