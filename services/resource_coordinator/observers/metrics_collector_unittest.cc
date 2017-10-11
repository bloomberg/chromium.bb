// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/metrics_collector.h"

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_test_harness.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"

namespace resource_coordinator {

const base::TimeDelta kTestMetricsReportDelayTimeout =
    kMetricsReportDelayTimeout + base::TimeDelta::FromSeconds(1);
const base::TimeDelta kTestMaxAudioSlientTimeout =
    kMaxAudioSlientTimeout + base::TimeDelta::FromSeconds(1);

class MetricsCollectorTest : public CoordinationUnitTestHarness {
 public:
  MetricsCollectorTest() : CoordinationUnitTestHarness() {}

  void SetUp() override {
    clock_ = new base::SimpleTestTickClock();
    // Sets a valid starting time.
    clock_->SetNowTicks(base::TimeTicks::Now());
    MetricsCollector* metrics_collector = new MetricsCollector();
    const_cast<base::TickClock*&>(metrics_collector->clock_) = clock_;
    coordination_unit_manager().RegisterObserver(
        base::WrapUnique(metrics_collector));
  }

  void TearDown() override { clock_ = nullptr; }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_->Advance(delta); }

  TestCoordinationUnitWrapper CreatePageCoordinationUnitWithClock() {
    auto page_cu = CreateCoordinationUnit(CoordinationUnitType::kPage);
    CoordinationUnitBase::ToPageCoordinationUnit(page_cu.get())
        ->SetClockForTest(std::unique_ptr<base::SimpleTestTickClock>(clock_));
    return page_cu;
  }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock* clock_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsCollectorTest);
};

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstAudioStartsUMA \
  DISABLED_FromBackgroundedToFirstAudioStartsUMA
#else
#define MAYBE_FromBackgroundedToFirstAudioStartsUMA \
  FromBackgroundedToFirstAudioStartsUMA
#endif
TEST_F(MetricsCollectorTest, MAYBE_FromBackgroundedToFirstAudioStartsUMA) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  page_cu->AddChild(frame_cu->id());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page was recently audible, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  AdvanceClock(kTestMaxAudioSlientTimeout);
  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page was not recently audible but it is not backgrounded, thus no
  // metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  AdvanceClock(kTestMaxAudioSlientTimeout);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  AdvanceClock(kTestMaxAudioSlientTimeout);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page becomes visible and then invisible again, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     2);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout \
  DISABLED_FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout
#else
#define MAYBE_FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout \
  FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout
#endif
TEST_F(MetricsCollectorTest,
       MAYBE_FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());

  page_cu->AddChild(frame_cu->id());

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstTitleUpdatedUMA \
  DISABLED_FromBackgroundedToFirstTitleUpdatedUMA
#else
#define MAYBE_FromBackgroundedToFirstTitleUpdatedUMA \
  FromBackgroundedToFirstTitleUpdatedUMA
#endif
TEST_F(MetricsCollectorTest, MAYBE_FromBackgroundedToFirstTitleUpdatedUMA) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     2);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout \
  DISABLED_FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout
#else
#define MAYBE_FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout \
  FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout
#endif
TEST_F(MetricsCollectorTest,
       MAYBE_FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_cu->SendEvent(mojom::Event::kTitleUpdated);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstAlertFiredUMA \
  DISABLED_FromBackgroundedToFirstAlertFiredUMA
#else
#define MAYBE_FromBackgroundedToFirstAlertFiredUMA \
  FromBackgroundedToFirstAlertFiredUMA
#endif
TEST_F(MetricsCollectorTest, MAYBE_FromBackgroundedToFirstAlertFiredUMA) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  page_cu->AddChild(frame_cu->id());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     0);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     2);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout \
  DISABLED_FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout
#else
#define MAYBE_FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout \
  FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout
#endif
TEST_F(MetricsCollectorTest,
       MAYBE_FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  page_cu->AddChild(frame_cu->id());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA \
  DISABLED_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA
#else
#define MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA \
  FromBackgroundedToFirstNonPersistentNotificationCreatedUMA
#endif
TEST_F(MetricsCollectorTest,
       MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  page_cu->AddChild(frame_cu->id());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 2);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout \
  DISABLED_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout
#else
#define MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout \
  FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout
#endif
TEST_F(
    MetricsCollectorTest,
    MAYBE_FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  page_cu->AddChild(frame_cu->id());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA \
  DISABLED_FromBackgroundedToFirstFaviconUpdatedUMA
#else
#define MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA \
  FromBackgroundedToFirstFaviconUpdatedUMA
#endif
TEST_F(MetricsCollectorTest, MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);

  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);

  page_cu->SetProperty(mojom::PropertyType::kVisible, true);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 2);
}

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if defined(OS_WIN)
#define MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout \
  DISABLED_FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout
#else
#define MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout \
  FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout
#endif
TEST_F(MetricsCollectorTest,
       MAYBE_FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout) {
  auto page_cu = CreatePageCoordinationUnitWithClock();
  coordination_unit_manager().OnCoordinationUnitCreated(page_cu.get());

  page_cu->SendEvent(mojom::Event::kNavigationCommitted);
  page_cu->SetProperty(mojom::PropertyType::kVisible, false);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_cu->SendEvent(mojom::Event::kFaviconUpdated);
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
}

}  // namespace resource_coordinator
