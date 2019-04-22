// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_signal_generator_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace performance_manager {

MATCHER_P3(IdentityMatches, cu_id, navigation_id, url, "") {
  return arg.page_cu_id == cu_id && arg.navigation_id == navigation_id &&
         arg.url == url;
}

class MockPageSignalGeneratorImpl : public PageSignalGeneratorImpl {
 public:
  void OnExpectedTaskQueueingDurationSample(
      ProcessNodeImpl* process_node) override {
    ++eqt_change_count_;
    // Forward the sample on to the actual implementation.
    PageSignalGeneratorImpl::OnExpectedTaskQueueingDurationSample(process_node);
  }

  size_t eqt_change_count() const { return eqt_change_count_; }

 private:
  size_t eqt_change_count_ = 0;
};

class MockPageSignalReceiverImpl
    : public resource_coordinator::mojom::PageSignalReceiver {
 public:
  explicit MockPageSignalReceiverImpl(
      resource_coordinator::mojom::PageSignalReceiverRequest request)
      : binding_(this, std::move(request)) {}
  ~MockPageSignalReceiverImpl() override = default;

  // resource_coordinator::mojom::PageSignalReceiver implementation.
  void NotifyPageAlmostIdle(const resource_coordinator::PageNavigationIdentity&
                                page_navigation_id) override {}
  void SetLifecycleState(
      const resource_coordinator::PageNavigationIdentity& page_navigation_id,
      resource_coordinator::mojom::LifecycleState) override {}
  MOCK_METHOD2(SetExpectedTaskQueueingDuration,
               void(const resource_coordinator::PageNavigationIdentity&
                        page_navigation_id,
                    base::TimeDelta duration));
  MOCK_METHOD1(NotifyNonPersistentNotificationCreated,
               void(const resource_coordinator::PageNavigationIdentity&
                        page_navigation_id));
  MOCK_METHOD4(OnLoadTimePerformanceEstimate,
               void(const resource_coordinator::PageNavigationIdentity&
                        page_navigation_id,
                    base::TimeDelta load_duration,
                    base::TimeDelta cpu_usage_estimate,
                    uint64_t private_footprint_kb_estimate));

 private:
  mojo::Binding<resource_coordinator::mojom::PageSignalReceiver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockPageSignalReceiverImpl);
};

using MockPageSignalReceiver =
    ::testing::StrictMock<MockPageSignalReceiverImpl>;

class PageSignalGeneratorImplTest : public GraphTestHarness {
 protected:
  void SetUp() override {
    page_signal_generator_ = std::make_unique<MockPageSignalGeneratorImpl>();

    graph()->RegisterObserver(page_signal_generator_.get());

    page_almost_idle_decorator_ = std::make_unique<PageAlmostIdleDecorator>();
    // Ensure the PageAlmostIdleDecorator is installed as we depend on state
    // transition driven by it.
    graph()->RegisterObserver(page_almost_idle_decorator_.get());
  }
  void TearDown() override {
    PerformanceManagerClock::ResetClockForTesting();
    graph()->UnregisterObserver(page_almost_idle_decorator_.get());
    graph()->UnregisterObserver(page_signal_generator_.get());
  }

  MockPageSignalGeneratorImpl* page_signal_generator() {
    return page_signal_generator_.get();
  }

 private:
  std::unique_ptr<MockPageSignalGeneratorImpl> page_signal_generator_;
  std::unique_ptr<PageAlmostIdleDecorator> page_almost_idle_decorator_;

  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

TEST_F(PageSignalGeneratorImplTest,
       CalculatePageEQTForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  auto* page = mock_graph.page.get();

  // Expect an EQT notification on the page for the measurement taken from its
  // main frame.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver, SetExpectedTaskQueueingDuration(
                                   IdentityMatches(page->id(), 0u, ""),
                                   base::TimeDelta::FromMilliseconds(1)))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    // Sent two EQT measurements, one per process. One of them will be forwarded
    // to the page signal receiver.
    mock_graph.process->SetExpectedTaskQueueingDuration(
        base::TimeDelta::FromMilliseconds(1));
    mock_graph.other_process->SetExpectedTaskQueueingDuration(
        base::TimeDelta::FromMilliseconds(10));

    // Expect both of the signals to have been processed by the page signal
    // generator.
    EXPECT_EQ(2u, page_signal_generator()->eqt_change_count());

    run_loop.Run();
    ::testing::Mock::VerifyAndClear(&mock_receiver);
  }

  // Send an identical measurement and expect a repeated notification.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver, SetExpectedTaskQueueingDuration(
                                   IdentityMatches(page->id(), 0u, ""),
                                   base::TimeDelta::FromMilliseconds(1)))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    mock_graph.process->SetExpectedTaskQueueingDuration(
        base::TimeDelta::FromMilliseconds(1));
    EXPECT_EQ(3u, page_signal_generator()->eqt_change_count());
    run_loop.Run();
    ::testing::Mock::VerifyAndClear(&mock_receiver);
  }
}

TEST_F(PageSignalGeneratorImplTest, PageDataCorrectlyManaged) {
  auto* psg = page_signal_generator();

  // The observer relationship isn't required for testing GetPageData.
  EXPECT_EQ(0u, psg->page_data_.size());

  {
    MockSinglePageInSingleProcessGraph mock_graph(graph());

    auto* page_node = mock_graph.page.get();
    EXPECT_EQ(1u, psg->page_data_.count(page_node));
    EXPECT_TRUE(psg->GetPageData(page_node));
  }
  EXPECT_EQ(0u, psg->page_data_.size());
}

TEST_F(PageSignalGeneratorImplTest, NonPersistentNotificationCreatedEvent) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* frame_node = mock_graph.frame.get();

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver,
              NotifyNonPersistentNotificationCreated(
                  IdentityMatches(mock_graph.page->id(), 0u, "")))
      .WillOnce(::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Send a
  // resource_coordinator::mojom::Event::kNonPersistentNotificationCreated event
  // and wait for the receiver to get it.
  page_signal_generator()->OnNonPersistentNotificationCreated(frame_node);
  run_loop.Run();

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

namespace {

std::unique_ptr<ProcessResourceMeasurementBatch> CreateMeasurementBatch(
    base::TimeTicks start_time,
    size_t cpu_time_us,
    size_t private_fp_kb) {
  std::unique_ptr<ProcessResourceMeasurementBatch> batch =
      std::make_unique<ProcessResourceMeasurementBatch>();
  batch->batch_started_time = start_time;
  batch->batch_ended_time = start_time + base::TimeDelta::FromMicroseconds(10);

  ProcessResourceMeasurement measurement;
  measurement.pid = 1;
  measurement.cpu_usage = base::TimeDelta::FromMicroseconds(cpu_time_us);
  measurement.private_footprint_kb = static_cast<uint32_t>(private_fp_kb);
  batch->measurements.push_back(measurement);

  return batch;
}

}  // namespace

TEST_F(PageSignalGeneratorImplTest, OnLoadTimePerformanceEstimate) {
  PerformanceManagerClock::SetClockForTesting(task_env().GetMockTickClock());

  MockSinglePageInSingleProcessGraph mock_graph(graph());

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  auto* page_node = mock_graph.page.get();

  // Ensure that a navigation resets the performance measurement state.
  base::TimeTicks navigation_committed_time =
      PerformanceManagerClock::NowTicks();
  page_node->OnMainFrameNavigationCommitted(navigation_committed_time, 1,
                                            GURL("https://www.google.com/"));
  page_node->SetPageAlmostIdleForTesting(false);
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(page_node->page_almost_idle());
  page_node->SetPageAlmostIdleForTesting(true);
  EXPECT_TRUE(page_node->page_almost_idle());

  base::TimeTicks event_time = PerformanceManagerClock::NowTicks();

  // A measurement that starts before an initiating state change should not
  // result in a notification.
  mock_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time - base::TimeDelta::FromMicroseconds(2), 10, 100));

  // This measurement should result in a notification.
  mock_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 15, 150));

  // A second measurement after a notification has been generated shouldn't
  // generate a second notification.
  mock_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(4), 20, 200));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver,
                OnLoadTimePerformanceEstimate(
                    IdentityMatches(mock_graph.page->id(), 1u,
                                    GURL("https://www.google.com/")),
                    event_time - navigation_committed_time,
                    base::TimeDelta::FromMicroseconds(15), 150))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  // This ensures that no other notifications were caused by code up to this
  // point.
  ::testing::Mock::VerifyAndClear(&mock_receiver);

  // Advance time beyond the last measurement end time so that the SystemNode
  // sees time as advancing contiguously.
  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Make sure a second run around the state machine generates a second event.
  navigation_committed_time = PerformanceManagerClock::NowTicks();
  page_node->OnMainFrameNavigationCommitted(navigation_committed_time, 2,
                                            GURL("https://example.org/bobcat"));
  page_node->SetPageAlmostIdleForTesting(false);
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(page_node->page_almost_idle());
  page_node->SetPageAlmostIdleForTesting(true);
  EXPECT_TRUE(page_node->page_almost_idle());

  event_time = PerformanceManagerClock::NowTicks();

  // Dispatch another measurement and verify another notification is fired.
  mock_graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 25, 250));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver,
                OnLoadTimePerformanceEstimate(
                    IdentityMatches(mock_graph.page->id(), 2u,
                                    GURL("https://example.org/bobcat")),
                    event_time - navigation_committed_time,
                    base::TimeDelta::FromMicroseconds(25), 250))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

}  // namespace performance_manager
