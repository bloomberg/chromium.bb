// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class WebContentsCoordinationUnitImplTest
    : public CoordinationUnitImplTestBase {};

}  // namespace

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForSingleTabInSingleProcess) {
  MockSingleTabInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(40, cpu_usage);
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForMultipleTabsInSingleProcess) {
  MockMultipleTabsInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
  EXPECT_TRUE(cu_graph.other_tab->GetProperty(mojom::PropertyType::kCPUUsage,
                                              &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForSingleTabWithMultipleProcesses) {
  MockSingleTabWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage, 30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(70, cpu_usage);
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForMultipleTabsWithMultipleProcesses) {
  MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage, 40);
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage, 30);

  int64_t cpu_usage;
  EXPECT_TRUE(
      cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(20, cpu_usage);
  EXPECT_TRUE(cu_graph.other_tab->GetProperty(mojom::PropertyType::kCPUUsage,
                                              &cpu_usage));
  EXPECT_EQ(50, cpu_usage);
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabEQTForSingleTabInSingleProcess) {
  MockSingleTabInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);

  int64_t eqt;
  ASSERT_TRUE(cu_graph.tab->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabEQTForMultipleTabsInSingleProcess) {
  MockMultipleTabsInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, 1);

  int64_t eqt;
  ASSERT_TRUE(cu_graph.tab->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
  ASSERT_TRUE(cu_graph.other_tab->GetProperty(
      mojom::PropertyType::kExpectedTaskQueueingDuration, &eqt));
  EXPECT_EQ(1, eqt);
}

}  // namespace resource_coordinator
