// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/mock_coordination_unit_graphs.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {

// Observer used to make sure that signals are dispatched correctly.
class SystemObserver : public CoordinationUnitGraphObserver {
 public:
  // CoordinationUnitGraphObserver implementation:
  bool ShouldObserve(const CoordinationUnitBase* coordination_unit) override {
    auto cu_type = coordination_unit->id().type;
    return cu_type == CoordinationUnitType::kSystem;
  }

  void OnSystemEventReceived(const SystemCoordinationUnitImpl* system_cu,
                             const mojom::Event event) override {
    EXPECT_EQ(mojom::Event::kProcessCPUUsageReady, event);
    ++event_seen_count_;
  }

  size_t event_seen_count() const { return event_seen_count_; }

 private:
  size_t event_seen_count_ = 0;
};

class SystemCoordinationUnitImplTest : public CoordinationUnitTestHarness {
 public:
  void SetUp() override {
    ResourceCoordinatorClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

}  // namespace

TEST_F(SystemCoordinationUnitImplTest, OnProcessCPUUsageReady) {
  MockSinglePageInSingleProcessCoordinationUnitGraph cu_graph;
  SystemObserver observer;
  cu_graph.system->AddObserver(&observer);
  EXPECT_EQ(0u, observer.event_seen_count());
  cu_graph.system->OnProcessCPUUsageReady();
  EXPECT_EQ(1u, observer.event_seen_count());
}

}  // namespace resource_coordinator
