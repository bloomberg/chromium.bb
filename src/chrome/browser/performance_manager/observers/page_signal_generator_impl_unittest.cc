// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/observers/page_signal_generator_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator_test_utils.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"
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
  // Overridden from PageSignalGeneratorImpl.
  void OnProcessPropertyChanged(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override {
    if (property_type == resource_coordinator::mojom::PropertyType::
                             kExpectedTaskQueueingDuration)
      ++eqt_change_count_;
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
  void SetExpectedTaskQueueingDuration(
      const resource_coordinator::PageNavigationIdentity& page_navigation_id,
      base::TimeDelta duration) override {}
  void SetLifecycleState(
      const resource_coordinator::PageNavigationIdentity& page_navigation_id,
      resource_coordinator::mojom::LifecycleState) override {}
  MOCK_METHOD1(NotifyNonPersistentNotificationCreated,
               void(const resource_coordinator::PageNavigationIdentity&
                        page_navigation_id));
  MOCK_METHOD1(NotifyRendererIsBloated,
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
    std::unique_ptr<MockPageSignalGeneratorImpl> psg(
        std::make_unique<MockPageSignalGeneratorImpl>());

    page_signal_generator_ = psg.get();

    // The graph takes ownership of the psg.
    coordination_unit_graph()->RegisterObserver(std::move(psg));

    // Ensure the PageAlmostIdleDecorator is installed as we depend on state
    // transition driven by it.
    coordination_unit_graph()->RegisterObserver(
        std::make_unique<PageAlmostIdleDecorator>());
  }
  void TearDown() override { ResourceCoordinatorClock::ResetClockForTesting(); }

  MockPageSignalGeneratorImpl* page_signal_generator() {
    return page_signal_generator_;
  }

 private:
  MockPageSignalGeneratorImpl* page_signal_generator_ = nullptr;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

TEST_F(PageSignalGeneratorImplTest,
       CalculatePageEQTForSinglePageWithMultipleProcesses) {
  MockSinglePageWithMultipleProcessesGraph graph(coordination_unit_graph());

  graph.process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(1));
  graph.other_process->SetExpectedTaskQueueingDuration(
      base::TimeDelta::FromMilliseconds(10));

  EXPECT_EQ(2u, page_signal_generator()->eqt_change_count());
  // The |other_process| is not for the main frame so its EQT values does not
  // propagate to the page.
  int64_t eqt;
  EXPECT_TRUE(graph.page->GetExpectedTaskQueueingDuration(&eqt));
  EXPECT_EQ(1, eqt);
}

TEST_F(PageSignalGeneratorImplTest, PageDataCorrectlyManaged) {
  auto* psg = page_signal_generator();

  // The observer relationship isn't required for testing GetPageData.
  EXPECT_EQ(0u, psg->page_data_.size());

  {
    MockSinglePageInSingleProcessGraph graph(coordination_unit_graph());

    auto* page_node = graph.page.get();
    EXPECT_EQ(1u, psg->page_data_.count(page_node));
    EXPECT_TRUE(psg->GetPageData(page_node));
  }
  EXPECT_EQ(0u, psg->page_data_.size());
}

TEST_F(PageSignalGeneratorImplTest, NonPersistentNotificationCreatedEvent) {
  MockSinglePageInSingleProcessGraph graph(coordination_unit_graph());
  auto* frame_node = graph.frame.get();

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver, NotifyNonPersistentNotificationCreated(
                                 IdentityMatches(graph.page->id(), 0u, "")))
      .WillOnce(::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Send a
  // resource_coordinator::mojom::Event::kNonPersistentNotificationCreated event
  // and wait for the receiver to get it.
  page_signal_generator()->OnFrameEventReceived(
      frame_node,
      resource_coordinator::mojom::Event::kNonPersistentNotificationCreated);
  run_loop.Run();

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

TEST_F(PageSignalGeneratorImplTest, NotifyRendererIsBloatedSinglePage) {
  MockSinglePageInSingleProcessGraph graph(coordination_unit_graph());
  auto* process = graph.process.get();
  auto* psg = page_signal_generator();

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  psg->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver, NotifyRendererIsBloated(_));
  process->OnRendererIsBloated();
  run_loop.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

TEST_F(PageSignalGeneratorImplTest, NotifyRendererIsBloatedMultiplePages) {
  MockMultiplePagesInSingleProcessGraph graph(coordination_unit_graph());
  auto* process = graph.process.get();
  auto* psg = page_signal_generator();

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  psg->AddReceiver(std::move(mock_receiver_ptr));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_receiver, NotifyRendererIsBloated(_)).Times(0);
  process->OnRendererIsBloated();
  run_loop.RunUntilIdle();
  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

namespace {

resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr
CreateMeasurementBatch(base::TimeTicks start_time,
                       size_t cpu_time_us,
                       size_t private_fp_kb) {
  resource_coordinator::mojom::ProcessResourceMeasurementBatchPtr batch =
      resource_coordinator::mojom::ProcessResourceMeasurementBatch::New();
  batch->batch_started_time = start_time;
  batch->batch_ended_time = start_time + base::TimeDelta::FromMicroseconds(10);

  resource_coordinator::mojom::ProcessResourceMeasurementPtr measurement =
      resource_coordinator::mojom::ProcessResourceMeasurement::New();
  measurement->pid = 1;
  measurement->cpu_usage = base::TimeDelta::FromMicroseconds(cpu_time_us);
  measurement->private_footprint_kb = static_cast<uint32_t>(private_fp_kb);
  batch->measurements.push_back(std::move(measurement));

  return batch;
}

}  // namespace

TEST_F(PageSignalGeneratorImplTest, OnLoadTimePerformanceEstimate) {
  MockSinglePageInSingleProcessGraph graph(coordination_unit_graph());

  // Create a mock receiver and register it against the psg.
  resource_coordinator::mojom::PageSignalReceiverPtr mock_receiver_ptr;
  MockPageSignalReceiver mock_receiver(mojo::MakeRequest(&mock_receiver_ptr));
  page_signal_generator()->AddReceiver(std::move(mock_receiver_ptr));

  ResourceCoordinatorClock::SetClockForTesting(task_env().GetMockTickClock());
  task_env().FastForwardBy(base::TimeDelta::FromSeconds(1));

  auto* page_node = graph.page.get();

  // Ensure that a navigation resets the performance measurement state.
  base::TimeTicks navigation_committed_time =
      ResourceCoordinatorClock::NowTicks();
  page_node->OnMainFrameNavigationCommitted(navigation_committed_time, 1,
                                            "https://www.google.com/");
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(page_node->page_almost_idle());
  testing::PageAlmostIdleDecoratorTestUtils::DrivePageToLoadedAndIdle(
      page_node);
  EXPECT_TRUE(page_node->page_almost_idle());

  base::TimeTicks event_time = ResourceCoordinatorClock::NowTicks();

  // A measurement that starts before an initiating state change should not
  // result in a notification.
  graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time - base::TimeDelta::FromMicroseconds(2), 10, 100));

  // This measurement should result in a notification.
  graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 15, 150));

  // A second measurement after a notification has been generated shouldn't
  // generate a second notification.
  graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(4), 20, 200));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_receiver, OnLoadTimePerformanceEstimate(
                                   IdentityMatches(graph.page->id(), 1u,
                                                   "https://www.google.com/"),
                                   event_time - navigation_committed_time,
                                   base::TimeDelta::FromMicroseconds(15), 150))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  // This ensures that no other notifications were caused by code up to this
  // point.
  ::testing::Mock::VerifyAndClear(&mock_receiver);

  // Make sure a second run around the state machine generates a second event.
  navigation_committed_time = ResourceCoordinatorClock::NowTicks();
  page_node->OnMainFrameNavigationCommitted(navigation_committed_time, 2,
                                            "https://example.org/bobcat");
  task_env().FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(page_node->page_almost_idle());
  testing::PageAlmostIdleDecoratorTestUtils::DrivePageToLoadedAndIdle(
      page_node);
  EXPECT_TRUE(page_node->page_almost_idle());

  event_time = ResourceCoordinatorClock::NowTicks();

  // Dispatch another measurement and verify another notification is fired.
  graph.system->DistributeMeasurementBatch(CreateMeasurementBatch(
      event_time + base::TimeDelta::FromMicroseconds(2), 25, 250));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        mock_receiver,
        OnLoadTimePerformanceEstimate(
            IdentityMatches(graph.page->id(), 2u, "https://example.org/bobcat"),
            event_time - navigation_committed_time,
            base::TimeDelta::FromMicroseconds(25), 250))
        .WillOnce(
            ::testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    run_loop.Run();
  }

  ::testing::Mock::VerifyAndClear(&mock_receiver);
}

}  // namespace performance_manager
