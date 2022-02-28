// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/power_metrics_reporter.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/metrics/power/power_details_provider.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr const char* kBatteryDischargeRateHistogramName =
    "Power.BatteryDischargeRate2";
constexpr const char* kBatteryDischargeModeHistogramName =
    "Power.BatteryDischargeMode";
constexpr const char* kMainScreenBrightnessHistogramName =
    "Power.MainScreenBrightness2";
constexpr const char* kMainScreenBrightnessAvailableHistogramName =
    "Power.MainScreenBrightnessAvailable";

constexpr base::TimeDelta kExpectedMetricsCollectionInterval =
    base::Seconds(120);
constexpr double kTolerableTimeElapsedRatio = 0.10;
constexpr double kTolerablePositiveDrift = 1 + kTolerableTimeElapsedRatio;
constexpr double kTolerableNegativeDrift = 1 - kTolerableTimeElapsedRatio;

performance_monitor::ProcessMonitor::Metrics GetFakeProcessMetrics() {
  performance_monitor::ProcessMonitor::Metrics metrics;
  metrics.cpu_usage = 5;
  return metrics;
}

using UkmEntry = ukm::builders::PowerUsageScenariosIntervalData;

class PowerMetricsReporterAccess : public PowerMetricsReporter {
 public:
  // Expose members of PowerMetricsReporter publicly on
  // PowerMetricsReporterAccess.
  using PowerMetricsReporter::BatteryDischargeMode;
  using PowerMetricsReporter::ReportBatteryHistograms;
  using PowerMetricsReporter::ReportHistograms;
};

// TODO(sebmarchand|etiennep): Move this to a test util file.
class FakeBatteryLevelProvider : public BatteryLevelProvider {
 public:
  explicit FakeBatteryLevelProvider(
      std::queue<BatteryLevelProvider::BatteryState>* battery_states)
      : battery_states_(battery_states) {}

  void GetBatteryState(
      base::OnceCallback<void(const BatteryState&)> callback) override {
    DCHECK(!battery_states_->empty());
    BatteryLevelProvider::BatteryState state = battery_states_->front();
    battery_states_->pop();
    std::move(callback).Run(state);
  }

 private:
  raw_ptr<std::queue<BatteryLevelProvider::BatteryState>> battery_states_;
};

class TestProcessMonitor : public performance_monitor::ProcessMonitor {
 public:
  TestProcessMonitor() = default;
  TestProcessMonitor(const TestProcessMonitor& rhs) = delete;
  TestProcessMonitor& operator=(const TestProcessMonitor& rhs) = delete;
  ~TestProcessMonitor() override = default;

  // Call OnAggregatedMetricsSampled for all the observers with |metrics| as an
  // argument.
  void NotifyObserversForOnAggregatedMetricsSampled(const Metrics& metrics) {
    for (auto& obs : GetObserversForTesting())
      obs.OnAggregatedMetricsSampled(metrics);
  }
};

class TestUsageScenarioDataStoreImpl : public UsageScenarioDataStoreImpl {
 public:
  TestUsageScenarioDataStoreImpl() = default;
  TestUsageScenarioDataStoreImpl(const TestUsageScenarioDataStoreImpl& rhs) =
      delete;
  TestUsageScenarioDataStoreImpl& operator=(
      const TestUsageScenarioDataStoreImpl& rhs) = delete;
  ~TestUsageScenarioDataStoreImpl() override = default;

  IntervalData ResetIntervalData() override { return fake_data_; }

  void SetIntervalDataToReturn(IntervalData data) { fake_data_ = data; }

 private:
  IntervalData fake_data_;
};

class TestPowerDetailsProvider : public PowerDetailsProvider {
 public:
  TestPowerDetailsProvider() = default;
  explicit TestPowerDetailsProvider(double return_value)
      : brightness_to_return_(return_value) {}
  TestPowerDetailsProvider(const TestPowerDetailsProvider& rhs) = delete;
  TestPowerDetailsProvider& operator=(const TestPowerDetailsProvider& rhs) =
      delete;
  ~TestPowerDetailsProvider() override = default;

  absl::optional<double> GetMainScreenBrightnessLevel() override {
    return brightness_to_return_;
  }

  void set_brightness_to_return(absl::optional<double> brightness_to_return) {
    brightness_to_return_ = brightness_to_return;
  }

 private:
  absl::optional<double> brightness_to_return_;
};

// This doesn't use the typical {class being tested}Test name pattern because
// there's already a PowerMetricsReporterTest class in the chromeos namespace
// and this conflicts with it.
class PowerMetricsReporterUnitTest : public testing::Test {
 public:
  PowerMetricsReporterUnitTest() = default;
  PowerMetricsReporterUnitTest(const PowerMetricsReporterUnitTest& rhs) =
      delete;
  PowerMetricsReporterUnitTest& operator=(
      const PowerMetricsReporterUnitTest& rhs) = delete;
  ~PowerMetricsReporterUnitTest() override = default;

  void SetUp() override {
    // Start with a half-full battery.
    battery_states_.push(BatteryLevelProvider::BatteryState{
        1, 1, 0.5, true, base::TimeTicks::Now()});
    std::unique_ptr<BatteryLevelProvider> battery_provider =
        std::make_unique<FakeBatteryLevelProvider>(&battery_states_);
    battery_provider_ = battery_provider.get();
    base::RunLoop run_loop;
    power_metrics_reporter_ = std::make_unique<PowerMetricsReporter>(
        data_store_.AsWeakPtr(), std::move(battery_provider));
    power_metrics_reporter_->set_power_details_provider_for_testing(
        std::make_unique<TestPowerDetailsProvider>());
    power_metrics_reporter_->OnFirstSampleForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForNextSample(
      const performance_monitor::ProcessMonitor::Metrics& metrics) {
    base::RunLoop run_loop;
    power_metrics_reporter_->OnNextSampleForTesting(run_loop.QuitClosure());
    process_monitor_.NotifyObserversForOnAggregatedMetricsSampled(metrics);
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestProcessMonitor process_monitor_;
  TestUsageScenarioDataStoreImpl data_store_;
  std::queue<BatteryLevelProvider::BatteryState> battery_states_;
  std::unique_ptr<PowerMetricsReporter> power_metrics_reporter_;
  raw_ptr<BatteryLevelProvider> battery_provider_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

}  // namespace

TEST_F(PowerMetricsReporterUnitTest, UKMs) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  int fake_value = 42;
  fake_interval_data.uptime_at_interval_end = base::Hours(++fake_value);
  fake_interval_data.max_tab_count = ++fake_value;
  fake_interval_data.max_visible_window_count = ++fake_value;
  fake_interval_data.top_level_navigation_count = ++fake_value;
  fake_interval_data.tabs_closed_during_interval = ++fake_value;
  fake_interval_data.user_interaction_count = ++fake_value;
  fake_interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(++fake_value);
  fake_interval_data.time_with_open_webrtc_connection =
      base::Seconds(++fake_value);
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  fake_interval_data.source_id_for_longest_visible_origin_duration =
      base::Seconds(++fake_value);
  fake_interval_data.time_playing_video_in_visible_tab =
      base::Seconds(++fake_value);
  fake_interval_data.time_since_last_user_interaction_with_browser =
      base::Seconds(++fake_value);
  fake_interval_data.time_capturing_video = base::Seconds(++fake_value);
  fake_interval_data.time_playing_audio = base::Seconds(++fake_value);
  fake_interval_data.longest_visible_origin_duration =
      base::Seconds(++fake_value);
  fake_interval_data.sleep_events = 0;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Pretend that the battery has dropped by 20% in 2 minutes, for a rate of
  // 10% per minute.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.30, true, base::TimeTicks::Now()});

  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = ++fake_value * 0.01;
#if defined(OS_MAC)
  fake_metrics.idle_wakeups = ++fake_value;
  fake_metrics.package_idle_wakeups = ++fake_value;
  fake_metrics.energy_impact = ++fake_value;
#endif

  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id,
            fake_interval_data.source_id_for_longest_visible_origin);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName, 1000);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kCPUTimeMsName,
      kExpectedMetricsCollectionInterval.InSeconds() * 1000 *
          fake_metrics.cpu_usage);
#if defined(OS_MAC)
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kIdleWakeUpsName,
                                       fake_metrics.idle_wakeups);
  test_ukm_recorder_.ExpectEntryMetric(entries[0], UkmEntry::kPackageExitsName,
                                       fake_metrics.package_idle_wakeups);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kEnergyImpactScoreName, fake_metrics.energy_impact);
#endif
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kMaxTabCountName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.max_tab_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kMaxVisibleWindowCountName,
      ukm::GetExponentialBucketMin(fake_interval_data.max_visible_window_count,
                                   1.05));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTabClosedName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.tabs_closed_during_interval));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTopLevelNavigationEventsName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.top_level_navigation_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUserInteractionCountName,
      ukm::GetExponentialBucketMinForCounts1000(
          fake_interval_data.user_interaction_count));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kFullscreenVideoSingleMonitorSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_video_full_screen_single_monitor));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimeWithOpenWebRTCConnectionSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_with_open_webrtc_connection));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimePlayingVideoInVisibleTabName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_video_in_visible_tab));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kIntervalDurationSecondsName,
      kExpectedMetricsCollectionInterval.InSeconds());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kTimeSinceInteractionWithBrowserSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_since_last_user_interaction_with_browser));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kVideoCaptureSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_capturing_video));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBrowserShuttingDownName, false);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kPlayingAudioSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.time_playing_audio));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kOriginVisibilityTimeSecondsName,
      PowerMetricsReporter::GetBucketForSampleForTesting(
          fake_interval_data.longest_visible_origin_duration));
  EXPECT_EQ(nullptr,
            test_ukm_recorder_.GetEntryMetric(
                entries[0], UkmEntry::kMainScreenBrightnessPercentName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, false);

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBrowserShuttingDown) {
  const ukm::SourceId kTestSourceId =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.source_id_for_longest_visible_origin = kTestSourceId;
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  fake_metrics.cpu_usage = 0.5;
#if defined(OS_MAC)
  fake_metrics.idle_wakeups = 42;
  fake_metrics.package_idle_wakeups = 43;
  fake_metrics.energy_impact = 44;
#endif

  {
    auto fake_shutdown = browser_shutdown::SetShutdownTypeForTesting(
        browser_shutdown::ShutdownType::kBrowserExit);
    EXPECT_TRUE(browser_shutdown::HasShutdownStarted());
    WaitForNextSample(fake_metrics);
  }

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, kTestSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBrowserShuttingDownName, true);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsPluggedIn) {
  // Update the latest reported battery state to pretend that the system isn't
  // running on battery.
  power_metrics_reporter_->battery_state_for_testing().on_battery = false;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Push a battery state that indicates that the system is still not running
  // on battery.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kPluggedIn));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kPluggedIn, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateChanges) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // The initial battery state indicates that the system is running on battery,
  // pretends that this has changed.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 1.0, /* on_battery - */ false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kStateChanged));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kStateChanged, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateUnavailable) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // A nullopt battery value indicates that the battery level is unavailable.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, absl::nullopt, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(PowerMetricsReporterAccess::BatteryDischargeMode::
                               kChargeLevelUnavailable));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kChargeLevelUnavailable,
      1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoBattery) {
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Indicates that the system has no battery interface.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 0, 1.0, false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kNoBattery));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kNoBattery, 1);
}

#if defined(OS_MAC)
// Tests that on MacOS, a full |charge_level| while not plugged does not result
// in a kDischarging value emitted. See https://crbug.com/1249830.
TEST_F(PowerMetricsReporterUnitTest, UKMsMacFullyCharged) {
  // Set the initial battery level at 100%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 1.0;
  
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 1, 1.0, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(
          PowerMetricsReporterAccess::BatteryDischargeMode::kMacFullyCharged));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kMacFullyCharged, 1);
}
#endif  // defined(OS_MAC)

TEST_F(PowerMetricsReporterUnitTest, UKMsBatteryStateIncrease) {
  // Set the initial battery level at 50%.
  power_metrics_reporter_->battery_state_for_testing().charge_level = 0.5;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  // Set the new battery state at 75%.
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.75, true, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  // An increase in charge level is reported as an invalid discharge rate.
  EXPECT_FALSE(test_ukm_recorder_.EntryHasMetric(
      entries[0], UkmEntry::kBatteryDischargeRateName));
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kBatteryDischargeModeName,
      static_cast<int64_t>(PowerMetricsReporterAccess::BatteryDischargeMode::
                               kBatteryLevelIncreased));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kBatteryLevelIncreased,
      1);
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_ZeroWindow) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 0;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2.ZeroWindow",
                                       2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.ZeroWindow",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.ZeroWindow", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_AllTabsHidden) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 0;
  // Values below should be ignored.
  interval_data.time_capturing_video = base::Seconds(1);
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.AllTabsHidden", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.AllTabsHidden",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.AllTabsHidden", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_VideoCapture) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.VideoCapture", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.VideoCapture",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.VideoCapture", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_FullscreenVideo) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.top_level_navigation_count = 1;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.FullscreenVideo", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.FullscreenVideo",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.FullscreenVideo", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_EmbeddedVideo_NoNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.EmbeddedVideo_NoNavigation", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.EmbeddedVideo_NoNavigation",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.EmbeddedVideo_NoNavigation", 500,
      1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest,
       SuffixedHistograms_EmbeddedVideo_WithNavigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  interval_data.time_playing_video_in_visible_tab = base::Seconds(1);
  // Values below should be ignored.
  interval_data.time_playing_audio = base::Seconds(1);
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.EmbeddedVideo_WithNavigation", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.EmbeddedVideo_WithNavigation",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.EmbeddedVideo_WithNavigation", 500,
      1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Audio) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::Seconds(1);
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;
  interval_data.top_level_navigation_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2.Audio",
                                       2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.Audio",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.Audio", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Navigation) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 1;
  // Values below should be ignored.
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2.Navigation",
                                       2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.Navigation",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.Navigation", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Interaction) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 1;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeRate2.Interaction", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.Interaction",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.Interaction", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, SuffixedHistograms_Passive) {
  UsageScenarioDataStore::IntervalData interval_data;
  interval_data.max_tab_count = 1;
  interval_data.max_visible_window_count = 1;
  interval_data.time_capturing_video = base::TimeDelta();
  interval_data.time_playing_video_full_screen_single_monitor =
      base::TimeDelta();
  interval_data.time_playing_video_in_visible_tab = base::TimeDelta();
  interval_data.time_playing_audio = base::TimeDelta();
  interval_data.top_level_navigation_count = 0;
  interval_data.user_interaction_count = 0;

  PowerMetricsReporterAccess::ReportHistograms(
      interval_data, GetFakeProcessMetrics(),
      kExpectedMetricsCollectionInterval,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500);

  // Non-suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2", 2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample("PerformanceMonitor.AverageCPU2.Total",
                                       500, 1);

  // Suffixed histograms.
  histogram_tester_.ExpectUniqueSample("Power.BatteryDischargeRate2.Passive",
                                       2500, 1);
  histogram_tester_.ExpectUniqueSample(
      "Power.BatteryDischargeMode.Passive",
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
  histogram_tester_.ExpectUniqueSample(
      "PerformanceMonitor.AverageCPU2.Total.Passive", 500, 1);

  // Note: For simplicity, this test only verifies that one of the
  // PerformanceMonitor.* histograms is recorded correctly.
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooEarly) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) -
          base::Seconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500,
      PowerMetricsReporter::GetSuffixesForTesting(interval_data));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kInvalidInterval, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsEarly) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerableNegativeDrift) +
          base::Seconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500,
      PowerMetricsReporter::GetSuffixesForTesting(interval_data));

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsTooLate) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) +
          base::Seconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500,
      PowerMetricsReporter::GetSuffixesForTesting(interval_data));

  histogram_tester_.ExpectTotalCount(kBatteryDischargeRateHistogramName, 0);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kInvalidInterval, 1);
}

TEST_F(PowerMetricsReporterUnitTest, BatteryDischargeCaptureIsLate) {
  UsageScenarioDataStore::IntervalData interval_data;

  PowerMetricsReporterAccess::ReportBatteryHistograms(
      (kExpectedMetricsCollectionInterval * kTolerablePositiveDrift) -
          base::Seconds(1),
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 2500,
      PowerMetricsReporter::GetSuffixesForTesting(interval_data));

  histogram_tester_.ExpectUniqueSample(kBatteryDischargeRateHistogramName, 2500,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      kBatteryDischargeModeHistogramName,
      PowerMetricsReporterAccess::BatteryDischargeMode::kDischarging, 1);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsNoTab) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  fake_interval_data.max_tab_count = 0;
  fake_interval_data.max_visible_window_count = 0;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::kInvalidSourceId;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});

  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample(GetFakeProcessMetrics());

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, ukm::kInvalidSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kUptimeSecondsName,
      ukm::GetExponentialBucketMinForUserTiming(
          fake_interval_data.uptime_at_interval_end.InSeconds()));
}

TEST_F(PowerMetricsReporterUnitTest, DurationsLongerThanIntervalAreCapped) {
  UsageScenarioDataStore::IntervalData fake_interval_data;

  fake_interval_data.time_playing_video_full_screen_single_monitor =
      kExpectedMetricsCollectionInterval * 100;

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  WaitForNextSample(GetFakeProcessMetrics());

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  EXPECT_EQ(entries[0]->source_id, ukm::kInvalidSourceId);
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kFullscreenVideoSingleMonitorSecondsName,
      // Every value greater than |kExpectedMetricsCollectionInterval| should
      // fall in the same overflow bucket.
      PowerMetricsReporter::GetBucketForSampleForTesting(
          kExpectedMetricsCollectionInterval * 2));
}

TEST_F(PowerMetricsReporterUnitTest, UKMBrightnessLevel) {
  const double kFakeBrightnessLevel = 0.64;
  power_metrics_reporter_->set_power_details_provider_for_testing(
      std::make_unique<TestPowerDetailsProvider>(kFakeBrightnessLevel));
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      0, 0, 1.0, false, base::TimeTicks::Now()});

  UsageScenarioDataStore::IntervalData fake_interval_data;
  fake_interval_data.source_id_for_longest_visible_origin =
      ukm::ConvertToSourceId(42, ukm::SourceIdType::NAVIGATION_ID);
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  WaitForNextSample({});

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kMainScreenBrightnessPercentName, 60);
}

TEST_F(PowerMetricsReporterUnitTest, UKMsWithSleepEvent) {
  UsageScenarioDataStore::IntervalData fake_interval_data = {};
  fake_interval_data.sleep_events = 1;
  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  performance_monitor::ProcessMonitor::Metrics fake_metrics = {};
  WaitForNextSample(fake_metrics);

  auto entries = test_ukm_recorder_.GetEntriesByName(
      ukm::builders::PowerUsageScenariosIntervalData::kEntryName);
  EXPECT_EQ(1u, entries.size());

  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], UkmEntry::kDeviceSleptDuringIntervalName, true);
}

TEST_F(PowerMetricsReporterUnitTest, MainScreenBrightnessHistogram) {
  std::unique_ptr<PowerDetailsProvider> detail_provider =
      std::make_unique<TestPowerDetailsProvider>();
  TestPowerDetailsProvider* detail_provider_raw =
      static_cast<TestPowerDetailsProvider*>(detail_provider.get());
  power_metrics_reporter_->set_power_details_provider_for_testing(
      std::move(detail_provider));

  UsageScenarioDataStore::IntervalData fake_interval_data = {};

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);

  performance_monitor::ProcessMonitor::Metrics fake_metrics =
      GetFakeProcessMetrics();
  WaitForNextSample(fake_metrics);

  histogram_tester_.ExpectTotalCount(kMainScreenBrightnessHistogramName, 0);
  histogram_tester_.ExpectBucketCount(
      kMainScreenBrightnessAvailableHistogramName, false, 1);

  double kBrightnessValue = 0.5;
  detail_provider_raw->set_brightness_to_return(kBrightnessValue);

  task_environment_.FastForwardBy(kExpectedMetricsCollectionInterval);
  battery_states_.push(BatteryLevelProvider::BatteryState{
      1, 1, 0.50, true, base::TimeTicks::Now()});
  data_store_.SetIntervalDataToReturn(fake_interval_data);
  WaitForNextSample(fake_metrics);

  histogram_tester_.ExpectBucketCount(kMainScreenBrightnessHistogramName,
                                      kBrightnessValue * 100, 1);
  histogram_tester_.ExpectBucketCount(
      kMainScreenBrightnessAvailableHistogramName, true, 1);
}
