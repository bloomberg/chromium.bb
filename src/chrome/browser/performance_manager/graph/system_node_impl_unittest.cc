// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/system_node_impl.h"

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/graph/system_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

// Observer used to make sure that signals are dispatched correctly.
class SystemObserver : public SystemNodeImpl::ObserverDefaultImpl {
 public:
  void OnProcessCPUUsageReady(const SystemNode* system_node) override {
    ++system_event_seen_count_;
  }

  size_t system_event_seen_count() const { return system_event_seen_count_; }

 private:
  size_t system_event_seen_count_ = 0;
};

class SystemNodeImplTest : public GraphTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerClock::SetClockForTesting(&clock_);

    // Sets a valid starting time.
    clock_.SetNowTicks(base::TimeTicks::Now());
  }

  void TearDown() override { PerformanceManagerClock::ResetClockForTesting(); }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

 private:
  base::SimpleTestTickClock clock_;
};

std::unique_ptr<ProcessResourceMeasurementBatch> CreateMeasurementBatch(
    base::TimeTicks start_end_time,
    size_t num_processes,
    base::TimeDelta additional_cpu_time) {
  std::unique_ptr<ProcessResourceMeasurementBatch> batch =
      std::make_unique<ProcessResourceMeasurementBatch>();
  batch->batch_started_time = start_end_time;
  batch->batch_ended_time = start_end_time;

  for (size_t i = 1; i <= num_processes; ++i) {
    ProcessResourceMeasurement measurement;
    measurement.pid = i;
    measurement.cpu_usage =
        base::TimeDelta::FromMicroseconds(i * 10) + additional_cpu_time;
    measurement.private_footprint_kb = static_cast<uint32_t>(i * 100);

    batch->measurements.push_back(measurement);
  }

  return batch;
}

}  // namespace

TEST_F(SystemNodeImplTest, SafeDowncast) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  auto& sys = mock_graph.system;
  SystemNode* node = sys.get();
  EXPECT_EQ(sys.get(), SystemNodeImpl::FromNode(node));
  NodeBase* base = sys.get();
  EXPECT_EQ(base, NodeBase::FromNode(node));
  EXPECT_EQ(static_cast<Node*>(node), base->ToNode());
}

TEST_F(SystemNodeImplTest, DistributeMeasurementBatch) {
  SystemObserver observer;
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  graph()->AddSystemNodeObserver(&observer);

  EXPECT_EQ(0u, observer.system_event_seen_count());

  // Build and dispatch a measurement batch.
  base::TimeTicks start_time = base::TimeTicks::Now();
  mock_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time, 3, base::TimeDelta()));

  EXPECT_EQ(start_time, mock_graph.system->last_measurement_start_time());
  EXPECT_EQ(start_time, mock_graph.system->last_measurement_end_time());

  EXPECT_EQ(1u, observer.system_event_seen_count());

  // The first measurement batch results in a zero CPU usage for the processes.
  EXPECT_EQ(0, mock_graph.process->cpu_usage());
  EXPECT_EQ(100u, mock_graph.process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(10u),
            mock_graph.process->cumulative_cpu_usage());

  EXPECT_EQ(0, mock_graph.process->cpu_usage());
  EXPECT_EQ(200u, mock_graph.other_process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            mock_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(5),
            mock_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, mock_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(25),
            mock_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(250u, mock_graph.other_page->private_footprint_kb_estimate());

  // Dispatch another batch, and verify the CPUUsage is appropriately updated.
  mock_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time + base::TimeDelta::FromMicroseconds(10),
                             3, base::TimeDelta::FromMicroseconds(10)));
  EXPECT_EQ(100, mock_graph.process->cpu_usage());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            mock_graph.process->cumulative_cpu_usage());
  EXPECT_EQ(100, mock_graph.process->cpu_usage());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(30u),
            mock_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(10),
            mock_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, mock_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(40),
            mock_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(250u, mock_graph.other_page->private_footprint_kb_estimate());

  // Now test that a measurement batch that leaves out a process clears the
  // properties of that process - except for cumulative CPU, which can only
  // go forwards.
  mock_graph.system->DistributeMeasurementBatch(
      CreateMeasurementBatch(start_time + base::TimeDelta::FromMicroseconds(20),
                             1, base::TimeDelta::FromMicroseconds(310)));

  EXPECT_EQ(3000, mock_graph.process->cpu_usage());
  EXPECT_EQ(100u, mock_graph.process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(320u),
            mock_graph.process->cumulative_cpu_usage());

  EXPECT_EQ(3000, mock_graph.process->cpu_usage());
  EXPECT_EQ(0u, mock_graph.other_process->private_footprint_kb());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(30u),
            mock_graph.other_process->cumulative_cpu_usage());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(160),
            mock_graph.page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, mock_graph.page->private_footprint_kb_estimate());

  EXPECT_EQ(base::TimeDelta::FromMicroseconds(190),
            mock_graph.other_page->cumulative_cpu_usage_estimate());
  EXPECT_EQ(50u, mock_graph.other_page->private_footprint_kb_estimate());

  graph()->RemoveSystemNodeObserver(&observer);
}

namespace {

class LenientMockObserver : public SystemNodeImpl::Observer {
 public:
  LenientMockObserver() {}
  ~LenientMockObserver() override {}

  MOCK_METHOD1(OnSystemNodeAdded, void(const SystemNode*));
  MOCK_METHOD1(OnBeforeSystemNodeRemoved, void(const SystemNode*));
  MOCK_METHOD1(OnProcessCPUUsageReady, void(const SystemNode*));

  void SetNotifiedSystemNode(const SystemNode* system_node) {
    notified_system_node_ = system_node;
  }

  const SystemNode* TakeNotifiedSystemNode() {
    const SystemNode* node = notified_system_node_;
    notified_system_node_ = nullptr;
    return node;
  }

 private:
  const SystemNode* notified_system_node_ = nullptr;
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST_F(SystemNodeImplTest, ObserverWorks) {
  MockObserver obs;
  graph()->AddSystemNodeObserver(&obs);

  // Fetch the system node and expect a matching call to "OnSystemNodeAdded".
  EXPECT_CALL(obs, OnSystemNodeAdded(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  const SystemNode* system_node = graph()->FindOrCreateSystemNode();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  // "OnProcessCPUUsageReady" is tested explicitly in the above unittests.

  // Release the system node and expect a call to "OnBeforeSystemNodeRemoved".
  EXPECT_CALL(obs, OnBeforeSystemNodeRemoved(_))
      .WillOnce(Invoke(&obs, &MockObserver::SetNotifiedSystemNode));
  graph()->ReleaseSystemNodeForTesting();
  EXPECT_EQ(system_node, obs.TakeNotifiedSystemNode());

  graph()->RemoveSystemNodeObserver(&obs);
}

}  // namespace performance_manager
