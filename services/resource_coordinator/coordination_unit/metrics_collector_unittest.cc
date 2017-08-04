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

  auto tab_coordination_unit = CreateCoordinationUnit(tab_cu_id);
  auto frame_coordination_unit = CreateCoordinationUnit(frame_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(
      tab_coordination_unit.get());
  coordination_unit_manager().OnCoordinationUnitCreated(
      frame_coordination_unit.get());

  tab_coordination_unit->AddChild(frame_coordination_unit->id());

  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(true));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));

  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(false));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab was recently audible, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));

  AdvanceClock(base::TimeDelta::FromMinutes(1));
  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(true));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab was not recently audible but it is not backgrounded, thus no
  // metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     0);
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));

  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(false));
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);
}

TEST_F(MetricsCollectorTest, ReportMetricsOneTimeOnlyPerBackgrounded) {
  CoordinationUnitID tab_cu_id(CoordinationUnitType::kWebContents,
                               std::string());
  CoordinationUnitID frame_cu_id(CoordinationUnitType::kFrame, std::string());

  auto tab_coordination_unit = CreateCoordinationUnit(tab_cu_id);
  auto frame_coordination_unit = CreateCoordinationUnit(frame_cu_id);
  coordination_unit_manager().OnCoordinationUnitCreated(
      tab_coordination_unit.get());
  coordination_unit_manager().OnCoordinationUnitCreated(
      frame_coordination_unit.get());

  tab_coordination_unit->AddChild(frame_coordination_unit->id());

  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(false));

  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab was not recently audible and it is backgrounded, thus metrics
  // recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);

  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // Only record the metrics once.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     1);

  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(true));
  tab_coordination_unit->SetProperty(mojom::PropertyType::kVisible,
                                     base::MakeUnique<base::Value>(false));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(false));
  AdvanceClock(base::TimeDelta::FromSeconds(61));
  frame_coordination_unit->SetProperty(mojom::PropertyType::kAudible,
                                       base::MakeUnique<base::Value>(true));
  // The tab becomes visible and then invisible again, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstAudioStartsUMA,
                                     2);
}

}  // namespace resource_coordinator
