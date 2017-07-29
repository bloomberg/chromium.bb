// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_types.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

class CoordinationUnitGraphObserverTest : public CoordinationUnitImplTestBase {
};

class TestCoordinationUnitGraphObserver : public CoordinationUnitGraphObserver {
 public:
  TestCoordinationUnitGraphObserver()
      : child_added_count_(0u),
        parent_added_count_(0u),
        coordination_unit_created_count_(0u),
        property_changed_count_(0u),
        child_removed_count_(0u),
        parent_removed_count_(0u),
        coordination_unit_destroyed_count_(0u) {}

  size_t child_added_count() { return child_added_count_; }
  size_t parent_added_count() { return parent_added_count_; }
  size_t coordination_unit_created_count() {
    return coordination_unit_created_count_;
  }
  size_t property_changed_count() { return property_changed_count_; }
  size_t child_removed_count() { return child_removed_count_; }
  size_t parent_removed_count() { return parent_removed_count_; }
  size_t coordination_unit_destroyed_count() {
    return coordination_unit_destroyed_count_;
  }

  // Overridden from CoordinationUnitGraphObserver.
  bool ShouldObserve(const CoordinationUnitImpl* coordination_unit) override {
    return coordination_unit->id().type == CoordinationUnitType::kFrame;
  }
  void OnCoordinationUnitCreated(
      const CoordinationUnitImpl* coordination_unit) override {
    ++coordination_unit_created_count_;
  }
  void OnChildAdded(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* child_coordination_unit) override {
    ++child_added_count_;
  }
  void OnParentAdded(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* parent_coordination_unit) override {
    ++parent_added_count_;
  }

  void OnPropertyChanged(const CoordinationUnitImpl* coordination_unit,
                         const mojom::PropertyType property_type,
                         const base::Value& value) override {
    ++property_changed_count_;
  }
  void OnChildRemoved(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* former_child_coordination_unit) override {
    ++child_removed_count_;
  }
  void OnParentRemoved(
      const CoordinationUnitImpl* coordination_unit,
      const CoordinationUnitImpl* former_parent_coordination_unit) override {
    ++parent_removed_count_;
  }
  void OnBeforeCoordinationUnitDestroyed(
      const CoordinationUnitImpl* coordination_unit) override {
    ++coordination_unit_destroyed_count_;
  }

 private:
  size_t child_added_count_;
  size_t parent_added_count_;
  size_t coordination_unit_created_count_;
  size_t property_changed_count_;
  size_t child_removed_count_;
  size_t parent_removed_count_;
  size_t coordination_unit_destroyed_count_;
};

}  // namespace

TEST_F(CoordinationUnitGraphObserverTest, CallbacksInvoked) {
  EXPECT_TRUE(coordination_unit_manager().observers_for_testing().empty());
  coordination_unit_manager().RegisterObserver(
      base::MakeUnique<TestCoordinationUnitGraphObserver>());
  EXPECT_EQ(1u, coordination_unit_manager().observers_for_testing().size());

  TestCoordinationUnitGraphObserver* observer =
      static_cast<TestCoordinationUnitGraphObserver*>(
          coordination_unit_manager().observers_for_testing()[0].get());

  CoordinationUnitID process_cu_id(CoordinationUnitType::kProcess,
                                   std::string());
  CoordinationUnitID root_frame_cu_id(CoordinationUnitType::kFrame,
                                      std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto process_coordination_unit = CreateCoordinationUnit(process_cu_id);
  auto root_frame_coordination_unit = CreateCoordinationUnit(root_frame_cu_id);
  auto frame_coordination_unit = CreateCoordinationUnit(frame_cu_id);

  coordination_unit_manager().OnCoordinationUnitCreated(
      process_coordination_unit.get());
  coordination_unit_manager().OnCoordinationUnitCreated(
      root_frame_coordination_unit.get());
  coordination_unit_manager().OnCoordinationUnitCreated(
      frame_coordination_unit.get());
  EXPECT_EQ(2u, observer->coordination_unit_created_count());

  // The registered observer will only observe the events that happen to
  // |root_frame_coordination_unit| and |frame_coordination_unit| because
  // they are CoordinationUnitType::kFrame.
  // OnAddParent will called for |root_frame_coordination_unit|.
  process_coordination_unit->AddChild(root_frame_coordination_unit->id());
  // OnAddParent will called for |frame_coordination_unit|.
  process_coordination_unit->AddChild(frame_coordination_unit->id());
  // OnAddChild will called for |root_frame_coordination_unit| and
  // OnAddParent will called for |frame_coordination_unit|.
  root_frame_coordination_unit->AddChild(frame_coordination_unit->id());
  EXPECT_EQ(1u, observer->child_added_count());
  EXPECT_EQ(3u, observer->parent_added_count());

  // The registered observer will only observe the events that happen to
  // |root_frame_coordination_unit| and |frame_coordination_unit| because
  // they are CoordinationUnitType::kFrame.
  // OnRemoveParent will called for |root_frame_coordination_unit|.
  process_coordination_unit->RemoveChild(root_frame_coordination_unit->id());
  // OnRemoveParent will called for |frame_coordination_unit|.
  process_coordination_unit->RemoveChild(frame_coordination_unit->id());
  // OnRemoveChild will called for |root_frame_coordination_unit| and
  // OnRemoveParent will called for |frame_coordination_unit|.
  root_frame_coordination_unit->RemoveChild(frame_coordination_unit->id());
  EXPECT_EQ(1u, observer->child_removed_count());
  EXPECT_EQ(3u, observer->parent_removed_count());

  // The registered observer will only observe the events that happen to
  // |root_frame_coordination_unit| and |frame_coordination_unit| because
  // they are CoordinationUnitType::kFrame, so OnPropertyChanged
  // will only be called for |root_frame_coordination_unit|.
  root_frame_coordination_unit->SetProperty(mojom::PropertyType::kTest,
                                            base::MakeUnique<base::Value>(42));
  process_coordination_unit->SetProperty(mojom::PropertyType::kTest,
                                         base::MakeUnique<base::Value>(42));
  EXPECT_EQ(1u, observer->property_changed_count());

  coordination_unit_manager().OnBeforeCoordinationUnitDestroyed(
      process_coordination_unit.get());
  coordination_unit_manager().OnBeforeCoordinationUnitDestroyed(
      root_frame_coordination_unit.get());
  coordination_unit_manager().OnBeforeCoordinationUnitDestroyed(
      frame_coordination_unit.get());
  EXPECT_EQ(2u, observer->coordination_unit_destroyed_count());
}

}  // namespace resource_coordinator
