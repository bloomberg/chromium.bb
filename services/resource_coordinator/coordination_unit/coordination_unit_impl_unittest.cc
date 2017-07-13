// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class CoordinationUnitImplTest : public CoordinationUnitImplTestBase {};

class TestCoordinationUnit : public mojom::CoordinationPolicyCallback {
 public:
  TestCoordinationUnit(CoordinationUnitProviderImpl* provider,
                       const CoordinationUnitType& type,
                       const std::string& id)
      : binding_(this) {
    CHECK(provider);

    CoordinationUnitID new_cu_id(type, id);
    mojom::CoordinationUnitPtr coordination_unit;
    provider->CreateCoordinationUnit(mojo::MakeRequest(&coordination_unit_),
                                     new_cu_id);

    base::RunLoop callback;
    SetGetIDClosure(callback.QuitClosure());
    coordination_unit_->SetCoordinationPolicyCallback(GetPolicyCallback());
    // Forces us to wait for the creation of the CUID to finish.
    coordination_unit_->GetID(base::Bind(&TestCoordinationUnit::GetIDCallback,
                                         base::Unretained(this)));

    callback.Run();
  }

  void GetIDCallback(const CoordinationUnitID& cu_id) {
    id_ = cu_id;
    get_id_closure_.Run();
  }

  void SetGetIDClosure(const base::Closure& get_id_closure) {
    get_id_closure_ = get_id_closure;
  }

  void SetPolicyClosure(const base::Closure& policy_closure) {
    policy_update_closure_ = policy_closure;
  }

  mojom::CoordinationPolicyCallbackPtr GetPolicyCallback() {
    mojom::CoordinationPolicyCallbackPtr callback_proxy;
    binding_.Bind(mojo::MakeRequest(&callback_proxy));
    return callback_proxy;
  }

  // The CU will always send policy updates on events (including parent events)
  void ForcePolicyUpdates() {
    base::RunLoop callback;
    SetPolicyClosure(callback.QuitClosure());
    mojom::EventPtr event = mojom::Event::New();
    event->type = mojom::EventType::kTestEvent;
    coordination_unit_->SendEvent(std::move(event));
    callback.Run();
  }

  const mojom::CoordinationUnitPtr& interface() const {
    return coordination_unit_;
  }

  const CoordinationUnitID& id() const { return id_; }

  // mojom::CoordinationPolicyCallback:
  void SetCoordinationPolicy(
      resource_coordinator::mojom::CoordinationPolicyPtr policy) override {
    if (policy_update_closure_) {
      policy_update_closure_.Run();
    }
  }

 private:
  base::Closure policy_update_closure_;
  base::Closure get_id_closure_;

  mojo::Binding<mojom::CoordinationPolicyCallback> binding_;
  mojom::CoordinationUnitPtr coordination_unit_;
  CoordinationUnitID id_;
};

}  // namespace

TEST_F(CoordinationUnitImplTest, BasicPolicyCallback) {
  TestCoordinationUnit test_coordination_unit(
      provider(), CoordinationUnitType::kWebContents, "test_id");
  test_coordination_unit.ForcePolicyUpdates();
}

TEST_F(CoordinationUnitImplTest, AddChild) {
  TestCoordinationUnit parent_unit(
      provider(), CoordinationUnitType::kWebContents, "parent_unit");

  TestCoordinationUnit child_unit(
      provider(), CoordinationUnitType::kWebContents, "child_unit");

  child_unit.ForcePolicyUpdates();
  parent_unit.ForcePolicyUpdates();

  {
    base::RunLoop callback;
    child_unit.SetPolicyClosure(callback.QuitClosure());
    parent_unit.interface()->AddChild(child_unit.id());
    callback.Run();
  }

  {
    base::RunLoop parent_callback;
    base::RunLoop child_callback;
    parent_unit.SetPolicyClosure(parent_callback.QuitClosure());
    child_unit.SetPolicyClosure(child_callback.QuitClosure());

    // This event should force the policy to recalculated for all children.
    mojom::EventPtr event = mojom::Event::New();
    event->type = mojom::EventType::kTestEvent;
    parent_unit.interface()->SendEvent(std::move(event));

    parent_callback.Run();
    child_callback.Run();
  }
}

TEST_F(CoordinationUnitImplTest, RemoveChild) {
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

TEST_F(CoordinationUnitImplTest, CyclicGraphUnits) {
  TestCoordinationUnit parent_unit(
      provider(), CoordinationUnitType::kWebContents, std::string());

  TestCoordinationUnit child_unit(
      provider(), CoordinationUnitType::kWebContents, std::string());

  child_unit.ForcePolicyUpdates();
  parent_unit.ForcePolicyUpdates();

  {
    base::RunLoop callback;
    child_unit.SetPolicyClosure(callback.QuitClosure());
    parent_unit.interface()->AddChild(child_unit.id());
    callback.Run();
  }

  // This should fail, due to the existing child-parent relationship.
  // Otherwise we end up with infinite recursion and crash when recalculating
  // policies below.
  child_unit.interface()->AddChild(parent_unit.id());

  {
    base::RunLoop parent_callback;
    base::RunLoop child_callback;
    parent_unit.SetPolicyClosure(parent_callback.QuitClosure());
    child_unit.SetPolicyClosure(child_callback.QuitClosure());

    // This event should force the policy to recalculated for all children.
    mojom::EventPtr event = mojom::Event::New();
    event->type = mojom::EventType::kTestEvent;
    parent_unit.interface()->SendEvent(std::move(event));

    parent_callback.Run();
    child_callback.Run();
  }
}

TEST_F(CoordinationUnitImplTest, GetSetProperty) {
  auto coordination_unit =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);

  // An empty value should be returned if property is not found
  EXPECT_EQ(base::Value(),
            coordination_unit->GetProperty(mojom::PropertyType::kTest));

  // Perform a valid storage property set
  coordination_unit->SetProperty(mojom::PropertyType::kTest,
                                 base::MakeUnique<base::Value>(41));
  EXPECT_EQ(1u, coordination_unit->properties_for_testing().size());
  EXPECT_EQ(base::Value(41),
            coordination_unit->GetProperty(mojom::PropertyType::kTest));
}

TEST_F(CoordinationUnitImplTest,
       GetAssociatedCoordinationUnitsForSingleTabInSingleProcess) {
  MockSingleTabInSingleProcessCoordinationUnitGraph cu_graph;

  auto tabs_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(1u, tabs_associated_with_process.size());
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.tab.get()));

  auto processes_associated_with_tab =
      cu_graph.tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_tab.size());
  EXPECT_EQ(1u, processes_associated_with_tab.count(cu_graph.process.get()));
}

TEST_F(CoordinationUnitImplTest,
       GetAssociatedCoordinationUnitsForMultipleTabsInSingleProcess) {
  MockMultipleTabsInSingleProcessCoordinationUnitGraph cu_graph;

  auto tabs_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(2u, tabs_associated_with_process.size());
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.tab.get()));
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.other_tab.get()));

  auto processes_associated_with_tab =
      cu_graph.tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_tab.size());
  EXPECT_EQ(1u, processes_associated_with_tab.count(cu_graph.process.get()));

  auto processes_associated_with_other_tab =
      cu_graph.other_tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_other_tab.size());
  EXPECT_EQ(1u, processes_associated_with_tab.count(cu_graph.process.get()));
}

TEST_F(CoordinationUnitImplTest,
       GetAssociatedCoordinationUnitsForSingleTabWithMultipleProcesses) {
  MockSingleTabWithMultipleProcessesCoordinationUnitGraph cu_graph;

  auto tabs_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(1u, tabs_associated_with_process.size());
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.tab.get()));

  auto tabs_associated_with_other_process =
      cu_graph.other_process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(1u, tabs_associated_with_other_process.size());
  EXPECT_EQ(1u, tabs_associated_with_other_process.count(cu_graph.tab.get()));

  auto processes_associated_with_tab =
      cu_graph.tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(2u, processes_associated_with_tab.size());
  EXPECT_EQ(1u, processes_associated_with_tab.count(cu_graph.process.get()));
  EXPECT_EQ(1u,
            processes_associated_with_tab.count(cu_graph.other_process.get()));
}

TEST_F(CoordinationUnitImplTest,
       GetAssociatedCoordinationUnitsForMultipleTabsWithMultipleProcesses) {
  MockMultipleTabsWithMultipleProcessesCoordinationUnitGraph cu_graph;

  auto tabs_associated_with_process =
      cu_graph.process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(2u, tabs_associated_with_process.size());
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.tab.get()));
  EXPECT_EQ(1u, tabs_associated_with_process.count(cu_graph.other_tab.get()));

  auto tabs_associated_with_other_process =
      cu_graph.other_process->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kWebContents);
  EXPECT_EQ(1u, tabs_associated_with_other_process.size());
  EXPECT_EQ(1u,
            tabs_associated_with_other_process.count(cu_graph.other_tab.get()));

  auto processes_associated_with_tab =
      cu_graph.tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(1u, processes_associated_with_tab.size());
  EXPECT_EQ(1u, processes_associated_with_tab.count(cu_graph.process.get()));

  auto processes_associated_with_other_tab =
      cu_graph.other_tab->GetAssociatedCoordinationUnitsOfType(
          CoordinationUnitType::kProcess);
  EXPECT_EQ(2u, processes_associated_with_other_tab.size());
  EXPECT_EQ(1u,
            processes_associated_with_other_tab.count(cu_graph.process.get()));
  EXPECT_EQ(1u, processes_associated_with_other_tab.count(
                    cu_graph.other_process.get()));
}

}  // namespace resource_coordinator
