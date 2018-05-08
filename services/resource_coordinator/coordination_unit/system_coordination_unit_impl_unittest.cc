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
class SystemAndProcessObserver : public CoordinationUnitGraphObserver {
 public:
  // CoordinationUnitGraphObserver implementation:
  bool ShouldObserve(const CoordinationUnitBase* coordination_unit) override {
    auto cu_type = coordination_unit->id().type;
    return cu_type == CoordinationUnitType::kSystem;
  }

  void OnSystemEventReceived(const SystemCoordinationUnitImpl* system_cu,
                             const mojom::Event event) override {
    EXPECT_EQ(mojom::Event::kProcessCPUUsageReady, event);
    ++system_event_seen_count_;
  }

  void OnProcessPropertyChanged(const ProcessCoordinationUnitImpl* process_cu,
                                const mojom::PropertyType property,
                                int64_t value) override {
    ++process_property_change_seen_count_;
  }

  size_t system_event_seen_count() const { return system_event_seen_count_; }
  size_t process_property_change_seen_count() const {
    return process_property_change_seen_count_;
  }

 private:
  size_t system_event_seen_count_ = 0;
  size_t process_property_change_seen_count_ = 0;
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
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph;
  SystemAndProcessObserver observer;
  cu_graph.system->AddObserver(&observer);
  EXPECT_EQ(0u, observer.system_event_seen_count());
  cu_graph.system->OnProcessCPUUsageReady();
  EXPECT_EQ(1u, observer.system_event_seen_count());
}

TEST_F(SystemCoordinationUnitImplTest, DistributeMeasurementBatch) {
  MockMultiplePagesWithMultipleProcessesCoordinationUnitGraph cu_graph;
  SystemAndProcessObserver observer;
  cu_graph.system->AddObserver(&observer);
  cu_graph.process->AddObserver(&observer);
  cu_graph.other_process->AddObserver(&observer);

  EXPECT_EQ(0u, observer.system_event_seen_count());

  // Build and dispatch a measurement batch.
  mojom::ProcessResourceMeasurementBatchPtr batch =
      mojom::ProcessResourceMeasurementBatch::New();

  for (size_t i = 1; i <= 3; ++i) {
    mojom::ProcessResourceMeasurementPtr measurement =
        mojom::ProcessResourceMeasurement::New();
    measurement->pid = i;
    measurement->cpu_usage = static_cast<double>(i);
    measurement->private_footprint_kb = static_cast<uint32_t>(i);

    batch->measurements.push_back(std::move(measurement));
  }
  EXPECT_EQ(0U, observer.process_property_change_seen_count());
  cu_graph.system->DistributeMeasurementBatch(std::move(batch));
  // EXPECT_EQ(2U, observer.process_property_change_seen_count());
  EXPECT_EQ(1u, observer.system_event_seen_count());

  // Validate that the measurements were distributed to the process CUs.
  int64_t cpu_usage;
  EXPECT_TRUE(cu_graph.process->GetProperty(mojom::PropertyType::kCPUUsage,
                                            &cpu_usage));
  EXPECT_EQ(1000, cpu_usage);
  EXPECT_EQ(1u, cu_graph.process->private_footprint_kb());

  EXPECT_TRUE(cu_graph.other_process->GetProperty(
      mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(2000, cpu_usage);
  EXPECT_EQ(2u, cu_graph.other_process->private_footprint_kb());

  // Now test that a measurement batch that leaves out a process clears the
  // properties of that process.
  batch = mojom::ProcessResourceMeasurementBatch::New();
  mojom::ProcessResourceMeasurementPtr measurement =
      mojom::ProcessResourceMeasurement::New();
  measurement->pid = 1;
  measurement->cpu_usage = 30.0;
  measurement->private_footprint_kb = 30u;
  batch->measurements.push_back(std::move(measurement));

  cu_graph.system->DistributeMeasurementBatch(std::move(batch));

  EXPECT_TRUE(cu_graph.process->GetProperty(mojom::PropertyType::kCPUUsage,
                                            &cpu_usage));
  EXPECT_EQ(30000, cpu_usage);
  EXPECT_EQ(30u, cu_graph.process->private_footprint_kb());

  EXPECT_TRUE(cu_graph.other_process->GetProperty(
      mojom::PropertyType::kCPUUsage, &cpu_usage));
  EXPECT_EQ(0, cpu_usage);
  EXPECT_EQ(0u, cu_graph.other_process->private_footprint_kb());
}

}  // namespace resource_coordinator
