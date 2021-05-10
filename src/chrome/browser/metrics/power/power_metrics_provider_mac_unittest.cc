// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_provider_mac.h"

#include <queue>

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/power/battery_level_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr const char* kHistogramName = "Power.Mac.BatteryDischarge";
constexpr base::TimeDelta kMetricsCollectionInterval =
    base::TimeDelta::FromSeconds(60);
constexpr double kTolerableTimeElapsedRatio = 0.10;
constexpr double kTolerablePositiveDrift = 1 + kTolerableTimeElapsedRatio;
constexpr double kTolerableNegativeDrift = 1 - kTolerableTimeElapsedRatio;
}  // namespace

class PowerMetricsProviderTest : public testing::Test {
 public:
  class FakeBatteryLevelProvider : public BatteryLevelProvider {
   public:
    FakeBatteryLevelProvider(
        std::queue<BatteryLevelProvider::BatteryState>* battery_states)
        : battery_states_(battery_states) {}

    BatteryState GetBatteryState() override {
      DCHECK(!battery_states_->empty());
      BatteryLevelProvider::BatteryState state = battery_states_->front();
      battery_states_->pop();
      return state;
    }

   private:
    std::vector<BatteryInterface> GetBatteryInterfaceList() override {
      return {};
    }

    std::queue<BatteryLevelProvider::BatteryState>* battery_states_;
  };

  PowerMetricsProviderTest()
      : power_drain_recorder_(kMetricsCollectionInterval) {}
  void SetUp() override {
    // Setup |power_drain_recorder_| to use use the BatteryState values
    // provided by the tests instead of querying the system to build them.
    power_drain_recorder_.SetBatteryLevelProviderForTesting(
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_));
  }

  void TearDown() override {
    // All values should have been used in the test.
    ASSERT_TRUE(battery_states_.empty());
  }

  void ConsumeBatteryStates() {
    const size_t number_of_test_states = battery_states_.size();
    for (size_t i = 0; i < number_of_test_states; ++i) {
      power_drain_recorder_.RecordBatteryDischarge();
    }
  }

 protected:
  static constexpr int kScalingFactor = 10000;

  PowerDrainRecorder power_drain_recorder_;
  base::HistogramTester histogram_tester_;
  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  base::TimeTicks now_;
};

TEST_F(PowerMetricsProviderTest, BatteryDischargeOnPower) {
  // Two consecutive readings on power should not record a battery discharge.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, 1.00, false, now_});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.000, false, now_ + base::TimeDelta::FromMinutes(1)});

  ConsumeBatteryStates();
  histogram_tester_.ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeOnBattery) {
  constexpr double kFirstReading = 0.100;
  constexpr double kSecondReading = 0.098;

  // Two consecutive readings on battery should record a battery discharge.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true, now_});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      now_ + base::TimeDelta::FromMinutes(1)});

  ConsumeBatteryStates();
  histogram_tester_.ExpectUniqueSample(
      kHistogramName, (kFirstReading - kSecondReading) * kScalingFactor, 1);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeCapacityGrew) {
  // Capacity that grew between measurements means no discharge. No value should
  // be recorded.
  constexpr double kFirstReading = 0.098;
  constexpr double kSecondReading = 0.100;

  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true, now_});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      now_ + base::TimeDelta::FromMinutes(1)});

  ConsumeBatteryStates();
  histogram_tester_.ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeCaptureIsTooEarly) {
  constexpr double kFirstReading = 0.100;
  constexpr double kSecondReading = 0.098;

  const base::TimeTicks first_capture_time =
      now_ + base::TimeDelta::FromSeconds(60);
  const base::TimeTicks second_capture_time =
      first_capture_time +
      (kMetricsCollectionInterval * kTolerableNegativeDrift) -
      base::TimeDelta::FromSeconds(1);

  // If it took too long to record a value no recoding takes place.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true,
      first_capture_time});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      second_capture_time});

  ConsumeBatteryStates();
  histogram_tester_.ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeCaptureIsEarly) {
  constexpr double kFirstReading = 0.100;
  constexpr double kSecondReading = 0.098;

  const base::TimeTicks first_capture_time =
      now_ + base::TimeDelta::FromSeconds(60);
  const base::TimeTicks second_capture_time =
      first_capture_time +
      (kMetricsCollectionInterval * kTolerableNegativeDrift) +
      base::TimeDelta::FromSeconds(1);

  // The second recording came in just in time to not be counted as too early.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true,
      first_capture_time});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      second_capture_time});

  ConsumeBatteryStates();

  // The discharge rate is normalized to be representative over
  // |kMetricsCollectionInterval|.
  int discharge =
      base::ClampFloor((kFirstReading - kSecondReading) * kScalingFactor *
                       (kMetricsCollectionInterval /
                        (second_capture_time - first_capture_time)));
  histogram_tester_.ExpectUniqueSample(kHistogramName, discharge, 1);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeCaptureIsTooLate) {
  constexpr double kFirstReading = 0.100;
  constexpr double kSecondReading = 0.098;

  const base::TimeTicks first_capture_time = now_;

  // Go just slightly over the acceptable drift.
  const base::TimeTicks second_capture_time =
      first_capture_time +
      (kMetricsCollectionInterval * kTolerablePositiveDrift) +
      base::TimeDelta::FromSeconds(1);

  // If it took too long to record a value no recoding takes place.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true,
      first_capture_time});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      second_capture_time});

  ConsumeBatteryStates();
  histogram_tester_.ExpectTotalCount(kHistogramName, 0);
}

TEST_F(PowerMetricsProviderTest, BatteryDischargeCaptureIsLate) {
  constexpr double kFirstReading = 0.100;
  constexpr double kSecondReading = 0.098;

  const base::TimeTicks first_capture_time = now_;
  const base::TimeTicks second_capture_time =
      first_capture_time +
      (kMetricsCollectionInterval * kTolerablePositiveDrift) -
      base::TimeDelta::FromSeconds(1);

  // If it took longer to record the metric the value recorded is scaled to
  // normalize to one minute.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kFirstReading, true,
      first_capture_time});
  battery_states_.push(BatteryLevelProvider::BatteryState{
      /*interface_count=*/1, /*battery_count=*/1, kSecondReading, true,
      second_capture_time});

  ConsumeBatteryStates();

  // The discharge rate is normalized to be representative over
  // |kMetricsCollectionInterval|.
  int discharge =
      base::ClampFloor((kFirstReading - kSecondReading) * kScalingFactor *
                       (kMetricsCollectionInterval /
                        (second_capture_time - first_capture_time)));
  histogram_tester_.ExpectUniqueSample(kHistogramName, discharge, 1);
}
