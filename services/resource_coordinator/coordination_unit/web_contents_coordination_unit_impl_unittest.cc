// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
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

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage,
                                base::MakeUnique<base::Value>(40.0));

  EXPECT_EQ(base::Value(40.0),
            cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage));
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForMultipleTabsInSingleProcess) {
  MockMultipleTabsInSingleProcessCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage,
                                base::MakeUnique<base::Value>(40.0));

  EXPECT_EQ(base::Value(20.0),
            cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage));
  EXPECT_EQ(base::Value(20.0),
            cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage));
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForSingleTabWithMultipleProcesses) {
  MockSingleTabWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage,
                                base::MakeUnique<base::Value>(40.0));
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage,
                                      base::MakeUnique<base::Value>(30.0));

  EXPECT_EQ(base::Value(70.0),
            cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage));
}

TEST_F(WebContentsCoordinationUnitImplTest,
       CalculateTabCPUUsageForMultipleTabsWithMultipleProcesses) {
  MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph cu_graph;

  cu_graph.process->SetProperty(mojom::PropertyType::kCPUUsage,
                                base::MakeUnique<base::Value>(40.0));
  cu_graph.other_process->SetProperty(mojom::PropertyType::kCPUUsage,
                                      base::MakeUnique<base::Value>(30.0));

  EXPECT_EQ(base::Value(20.0),
            cu_graph.tab->GetProperty(mojom::PropertyType::kCPUUsage));
  EXPECT_EQ(base::Value(50.0),
            cu_graph.other_tab->GetProperty(mojom::PropertyType::kCPUUsage));
}

}  // namespace resource_coordinator
