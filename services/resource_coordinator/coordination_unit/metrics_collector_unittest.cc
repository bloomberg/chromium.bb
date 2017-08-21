// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/metrics_collector.h"

#include <string>

#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl_unittest_util.h"

namespace resource_coordinator {

class MetricsCollectorTest : public CoordinationUnitImplTestBase {
 public:
  void SetUp() override {
    MetricsCollector* metrics_collector = new MetricsCollector();
    const_cast<base::TickClock*&>(metrics_collector->clock_) = &clock_;
    coordination_unit_manager().RegisterObserver(
        base::WrapUnique(metrics_collector));
  }

 protected:
  void AdvanceClock(base::TimeDelta delta) { clock_.Advance(delta); }

  base::HistogramTester histogram_tester_;
  base::SimpleTestTickClock clock_;
};

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstAudioStartsUMA) {
  CoordinationUnitID tab_cu_id(CoordinationUnitType::kWebContents,
                               std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto web_contents_cu = CreateCoordinationUnit(tab_cu_id);
  auto frame_cu = CreateCoordinationUnit(frame_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());

  web_contents_cu->AddChild(frame_cu->id());

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

  AdvanceClock(base::TimeDelta::FromMinutes(1));
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was not recently audible but it is not backgrounded, thus no
  // metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
}

TEST_F(MetricsCollectorTest, ReportMetricsOneTimeOnlyPerBackgrounded) {
  CoordinationUnitID tab_cu_id(CoordinationUnitType::kWebContents,
                               std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto web_contents_cu = CreateCoordinationUnit(tab_cu_id);
  auto frame_cu = CreateCoordinationUnit(frame_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());

  web_contents_cu->AddChild(frame_cu->id());

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);

  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);

  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // Only record the metrics once.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);

  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, true);
  web_contents_cu->SetProperty(mojom::PropertyType::kVisible, false);
  frame_cu->SetProperty(mojom::PropertyType::kAudible, false);
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_cu->SetProperty(mojom::PropertyType::kAudible, true);
  // The tab becomes visible and then invisible again, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     2);
}

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstTitleUpdatedUMA) {
  CoordinationUnitID tab_cu_id(CoordinationUnitType::kWebContents,
                               std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto web_contents_cu = CreateCoordinationUnit(tab_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());

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

TEST_F(MetricsCollectorTest, FromBackgroundedToFirstAlertFiredUMA) {
  CoordinationUnitID tab_cu_id(CoordinationUnitType::kWebContents,
                               std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto web_contents_cu = CreateCoordinationUnit(tab_cu_id);
  auto frame_cu = CreateCoordinationUnit(frame_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(web_contents_cu.get());
  coordination_unit_manager().OnCoordinationUnitCreated(frame_cu.get());
  web_contents_cu->AddChild(frame_cu->id());

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

}  // namespace resource_coordinator
