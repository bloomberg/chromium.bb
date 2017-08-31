// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/metrics_collector.h"

#include <string>

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"

namespace resource_coordinator {

const base::TimeDelta kTestMetricsReportDelayTimeout =
    kMetricsReportDelayTimeout + base::TimeDelta::FromSeconds(1);
const base::TimeDelta kTestMaxAudioSlientTimeout =
    kMaxAudioSlientTimeout + base::TimeDelta::FromMinutes(1);

class MetricsCollectorTest : public CoordinationUnitImplTestBase {
 public:
  MetricsCollectorTest() : CoordinationUnitImplTestBase() {}

  void SetUp() override {
    metrics_collector_ = new MetricsCollector();
    const_cast<base::TickClock*&>(metrics_collector_->clock_) = &clock_;
    coordination_unit_manager().RegisterObserver(
        base::WrapUnique(metrics_collector_));
  }

  void CheckMetricsReportDelayTimeout(CoordinationUnitImpl* web_contents_cu) {
    ASSERT_TRUE(metrics_collector_->ShouldReportMetrics(
        CoordinationUnitImpl::ToWebContentsCoordinationUnit(web_contents_cu)));
  }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;
  MetricsCollector* metrics_collector_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsCollectorTest);
};

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstAudioStartsUMA) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());

  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was recently audible, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  AdvanceClock(kTestMaxAudioSlientTimeout);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was not recently audible but it is not backgrounded, thus no
  // metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  AdvanceClock(kTestMaxAudioSlientTimeout);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  AdvanceClock(kTestMaxAudioSlientTimeout);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab becomes visible and then invisible again, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstAudioStartsUMA5MinutesTimeout) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());

  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab is within 5 minutes after main frame navigation was committed, thus
  // no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
}

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstTitleUpdatedUMA) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The tab is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The tab is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  // The tab is within 5 minutes after main frame navigation was committed, thus
  // no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());
  web_contents_cu->SendEvent(mojom::Event::kTitleUpdated);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
}

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstAlertFiredUMA) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     0);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The tab is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The tab is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstAlertFiredUMA5MinutesTimeout) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  // The tab is within 5 minutes after main frame navigation was committed, thus
  // no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());
  frame_cu->SendEvent(mojom::Event::kAlertFired);
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAlertFiredUMA,
                                     1);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstNonPersistentNotificationCreatedUMA) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The tab is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The tab is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 2);
}

TEST_F(
    MetricsCollectorTest,
    FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  auto frame_cu = CreateCoordinationUnit(CoordinationUnitType::kFrame);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  // The tab is within 5 minutes after main frame navigation was committed, thus
  // no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());
  frame_cu->SendEvent(mojom::Event::kNonPersistentNotificationCreated);
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
}

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstFaviconUpdatedUMA) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The tab is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The tab is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 2);
}

TEST_F(MetricsCollectorTest,
       FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout) {
  auto web_contents_cu =
      CreateCoordinationUnit(CoordinationUnitType::kWebContents);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());

  web_contents_cu->SendEvent(mojom::Event::kNavigationCommitted);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  // The tab is within 5 minutes after main frame navigation was committed, thus
  // no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  CheckMetricsReportDelayTimeout(web_contents_cu.get());
  web_contents_cu->SendEvent(mojom::Event::kFaviconUpdated);
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
}

}  // namespace resource_coordinator
