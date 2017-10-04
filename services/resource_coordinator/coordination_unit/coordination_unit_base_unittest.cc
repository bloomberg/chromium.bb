// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class CoordinationUnitBaseTest : public CoordinationUnitTestHarness {};

using CoordinationUnitBaseDeathTest = CoordinationUnitBaseTest;

}  // namespace

TEST_F(CoordinationUnitBaseTest, AddChildBasic) {
  auto page_cu = CreateCoordinationUnit(CoordinationUnitType::kPage);
  auto frame1_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  auto frame2_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  auto frame3_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);

  page_cu->AddChild(frame1_cu->id());
  page_cu->AddChild(frame2_cu->id());
  page_cu->AddChild(frame3_cu->id());
  EXPECT_EQ(3u, page_cu->children().size());
}

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) && GTEST_HAS_DEATH_TEST
TEST_F(CoordinationUnitBaseDeathTest, AddChildOnCyclicReference) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  CoordinationUnitID frame1_cu_id(CoordinationUnitType::kFrame, std::string());
  CoordinationUnitID frame2_cu_id(CoordinationUnitType::kFrame, std::string());
  CoordinationUnitID frame3_cu_id(CoordinationUnitType::kFrame, std::string());

  auto frame1_cu = CreateCoordinationUnit(frame1_cu_id);
  auto frame2_cu = CreateCoordinationUnit(frame2_cu_id);
  auto frame3_cu = CreateCoordinationUnit(frame3_cu_id);

  frame1_cu->AddChild(frame2_cu->id());
  frame2_cu->AddChild(frame3_cu->id());
  // |frame3_cu| can't add |frame1_cu| because |frame1_cu| is an ancestor of
  // |frame3_cu|, and this will hit a DCHECK because of cyclic reference.
  EXPECT_DEATH(frame3_cu->AddChild(frame1_cu->id()), "");
}
#else
TEST_F(CoordinationUnitBaseTest, AddChildOnCyclicReference) {
  CoordinationUnitID frame1_cu_id(CoordinationUnitType::kFrame, std::string());
  CoordinationUnitID frame2_cu_id(CoordinationUnitType::kFrame, std::string());
  CoordinationUnitID frame3_cu_id(CoordinationUnitType::kFrame, std::string());

  auto frame1_cu = CreateCoordinationUnit(frame1_cu_id);
  auto frame2_cu = CreateCoordinationUnit(frame2_cu_id);
  auto frame3_cu = CreateCoordinationUnit(frame3_cu_id);

  frame1_cu->AddChild(frame2_cu->id());
  frame2_cu->AddChild(frame3_cu->id());
  frame3_cu->AddChild(frame1_cu->id());

  EXPECT_EQ(1u, frame1_cu->children().count(frame2_cu.get()));
  EXPECT_EQ(1u, frame2_cu->children().count(frame3_cu.get()));
  // |frame1_cu| was not added successfully because |frame1_cu| is one of the
  // ancestors of |frame3_cu|.
  EXPECT_EQ(0u, frame3_cu->children().count(frame1_cu.get()));
}
#endif  // (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) &&
        //     GTEST_HAS_DEATH_TEST

TEST_F(CoordinationUnitBaseTest, RemoveChild) {
  auto parent_coordination_unit =
      CreateCoordinationUnit(CoordinationUnitType::kFrame);
  auto child_coordination_unit =
      CreateCoordinationUnit(CoordinationUnitType::kFrame);

  // Parent-child relationships have not been established yet.
  EXPECT_EQ(0u, parent_coordination_unit->children().size());
  EXPECT_EQ(0u, parent_coordination_unit->parents().size());
  EXPECT_EQ(0u, child_coordination_unit->children().size());
  EXPECT_EQ(0u, child_coordination_unit->parents().size());

  parent_coordination_unit->AddChild(child_coordination_unit->id());

  // Ensure correct Parent-child relationships have been established.
  EXPECT_EQ(1u, parent_coordination_unit->children().size());
  EXPECT_EQ(0u, parent_coordination_unit->parents().size());
  EXPECT_EQ(0u, child_coordination_unit->children().size());
  EXPECT_EQ(1u, child_coordination_unit->parents().size());

  parent_coordination_unit->RemoveChild(child_coordination_unit->id());

  // Parent-child relationships should no longer exist.
  EXPECT_EQ(0u, parent_coordination_unit->children().size());
  EXPECT_EQ(0u, parent_coordination_unit->parents().size());
  EXPECT_EQ(0u, child_coordination_unit->children().size());
  EXPECT_EQ(0u, child_coordination_unit->parents().size());
}

TEST_F(CoordinationUnitBaseTest, GetSetProperty) {
  auto coordination_unit = CreateCoordinationUnit(CoordinationUnitType::kPage);

  // An empty value should be returned if property is not found
  int64_t test_value;
  EXPECT_FALSE(
      coordination_unit->GetProperty(mojom::PropertyType::kTest, &test_value));

  // Perform a valid storage property set
  coordination_unit->SetProperty(mojom::PropertyType::kTest, 41);
  EXPECT_EQ(1u, coordination_unit->properties_for_testing().size());
  EXPECT_TRUE(
      coordination_unit->GetProperty(mojom::PropertyType::kTest, &test_value));
  EXPECT_EQ(41, test_value);
}

TEST_F(CoordinationUnitBaseTest,
       GetAssociatedCoordinationUnitsForSinglePageInSingleProcess) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;

  auto pages_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(1u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.page.get()));

  auto processes_associated_with_page =
      cu_graph.page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(cu_graph.process.get()));
}

TEST_F(CoordinationUnitBaseTest,
       GetAssociatedCoordinationUnitsForMultiplePagesInSingleProcess) {
  MockMultiplePagesInSingleProcessCoordinationUnitGraph cu_graph;

  auto pages_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(2u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.page.get()));
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.other_page.get()));

  auto processes_associated_with_page =
      cu_graph.page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(cu_graph.process.get()));

  auto processes_associated_with_other_page =
      cu_graph.other_page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_other_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(cu_graph.process.get()));
}

TEST_F(CoordinationUnitBaseTest,
       GetAssociatedCoordinationUnitsForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesCoordinationUnitGraph cu_graph;

  auto pages_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(1u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.page.get()));

  auto pages_associated_with_other_process =
      cu_graph.other_process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(1u, pages_associated_with_other_process.size());
  EXPECT_EQ(1u, pages_associated_with_other_process.count(cu_graph.page.get()));

  auto processes_associated_with_page =
      cu_graph.page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(2u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(cu_graph.process.get()));
  EXPECT_EQ(1u,
            processes_associated_with_page.count(cu_graph.other_process.get()));
}

TEST_F(CoordinationUnitBaseTest,
       GetAssociatedCoordinationUnitsForMultiplePagesWithMultipleProcesses) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph;

  auto pages_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(2u, pages_associated_with_process.size());
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.page.get()));
  EXPECT_EQ(1u, pages_associated_with_process.count(cu_graph.other_page.get()));

  auto pages_associated_with_other_process =
      cu_graph.other_process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kPage);
  EXPECT_EQ(1u, pages_associated_with_other_process.size());
  EXPECT_EQ(
      1u, pages_associated_with_other_process.count(cu_graph.other_page.get()));

  auto processes_associated_with_page =
      cu_graph.page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_page.size());
  EXPECT_EQ(1u, processes_associated_with_page.count(cu_graph.process.get()));

  auto processes_associated_with_other_page =
      cu_graph.other_page->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(2u, processes_associated_with_other_page.size());
  EXPECT_EQ(1u,
            processes_associated_with_other_page.count(cu_graph.process.get()));
  EXPECT_EQ(1u, processes_associated_with_other_page.count(
                    cu_graph.other_process.get()));
}

}  // namespace resource_coordinator
