// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_model_config_loader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

MonotoneCubicSpline CreateTestCurveFromTrainingData(
    const std::vector<TrainingDataPoint>& data) {
  CHECK_GT(data.size(), 1u);

  std::vector<double> xs;
  std::vector<double> ys;

  const auto& data_point = data[0];
  xs.push_back(data_point.ambient_log_lux);
  ys.push_back((data_point.brightness_old + data_point.brightness_new) / 2);

  for (size_t i = 1; i < data.size(); ++i) {
    xs.push_back(xs[i - 1] + 1);
    ys.push_back(ys[i - 1] + 1);
  }

  return MonotoneCubicSpline(xs, ys);
}

void CheckOptionalCurves(
    const base::Optional<MonotoneCubicSpline>& result_curve,
    const base::Optional<MonotoneCubicSpline>& expected_curve) {
  EXPECT_EQ(result_curve.has_value(), expected_curve.has_value());
  if (result_curve) {
    EXPECT_EQ(*result_curve, *expected_curve);
  }
}

// Fake testing trainer that has configuration status and validity of personal
// curve specified in the constructor.
class FakeTrainer : public Trainer {
 public:
  FakeTrainer(bool is_configured, bool is_personal_curve_valid)
      : is_configured_(is_configured),
        is_personal_curve_valid_(is_personal_curve_valid) {
    // If personal curve is valid, then the trainer must be configured.
    DCHECK(!is_personal_curve_valid_ || is_configured_);
  }
  ~FakeTrainer() override = default;

  // Trainer overrides:
  bool HasValidConfiguration() const override { return is_configured_; }

  bool SetInitialCurves(const MonotoneCubicSpline& global_curve,
                        const MonotoneCubicSpline& current_curve) override {
    DCHECK(is_configured_);
    global_curve_.emplace(global_curve);
    current_curve_.emplace(is_personal_curve_valid_ ? current_curve
                                                    : global_curve);
    return is_personal_curve_valid_;
  }

  MonotoneCubicSpline GetGlobalCurve() const override {
    DCHECK(is_configured_);
    DCHECK(global_curve_);
    return *global_curve_;
  }

  MonotoneCubicSpline GetCurrentCurve() const override {
    DCHECK(is_configured_);
    DCHECK(current_curve_);
    return *current_curve_;
  }

  MonotoneCubicSpline Train(
      const std::vector<TrainingDataPoint>& data) override {
    DCHECK(is_configured_);
    DCHECK(current_curve_);
    std::vector<TrainingDataPoint> used_data = data;

    // We need at least 2 points to create a MonotoneCubicSpline. Hence we
    // insert another one if |data| has only 1 point.
    if (data.size() == 1) {
      used_data.push_back(data[0]);
    }
    current_curve_.emplace(CreateTestCurveFromTrainingData(used_data));
    return *current_curve_;
  }

 private:
  bool is_configured_;
  bool is_personal_curve_valid_;
  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> current_curve_;

  DISALLOW_COPY_AND_ASSIGN(FakeTrainer);
};

class TestObserver : public Modeller::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override = default;

  // Modeller::Observer overrides:
  void OnModelTrained(const MonotoneCubicSpline& brightness_curve) override {
    personal_curve_.emplace(brightness_curve);
    trained_curve_received_ = true;
  }

  void OnModelInitialized(
      const base::Optional<MonotoneCubicSpline>& global_curve,
      const base::Optional<MonotoneCubicSpline>& personal_curve) override {
    model_initialized_ = true;
    if (global_curve)
      global_curve_.emplace(*global_curve);

    if (personal_curve)
      personal_curve_.emplace(*personal_curve);
  }

  base::Optional<MonotoneCubicSpline> global_curve() const {
    return global_curve_;
  }

  base::Optional<MonotoneCubicSpline> personal_curve() const {
    return personal_curve_;
  }

  void CheckStatus(bool is_model_initialized,
                   const base::Optional<MonotoneCubicSpline>& global_curve,
                   const base::Optional<MonotoneCubicSpline>& personal_curve) {
    EXPECT_EQ(is_model_initialized, model_initialized_);
    CheckOptionalCurves(global_curve, global_curve_);
    CheckOptionalCurves(personal_curve, personal_curve_);
  }

  bool trained_curve_received() { return trained_curve_received_; }

 private:
  bool model_initialized_ = false;
  base::Optional<MonotoneCubicSpline> global_curve_;
  base::Optional<MonotoneCubicSpline> personal_curve_;
  bool trained_curve_received_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class ModellerImplTest : public testing::Test {
 public:
  ModellerImplTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_ = profile_builder.Build();
  }

  ~ModellerImplTest() override {
    base::ThreadPool::GetInstance()->FlushForTesting();
  }

  // Sets up |modeller_| with a FakeTrainer.
  void SetUpModeller(bool is_trainer_configured, bool is_personal_curve_valid) {
    modeller_ = ModellerImpl::CreateForTesting(
        profile_.get(), &fake_als_reader_, &fake_brightness_monitor_,
        &fake_model_config_loader_, &user_activity_detector_,
        std::make_unique<FakeTrainer>(is_trainer_configured,
                                      is_personal_curve_valid),
        base::SequencedTaskRunnerHandle::Get(),
        thread_bundle_.GetMockTickClock());

    test_observer_ = std::make_unique<TestObserver>();
    modeller_->AddObserver(test_observer_.get());
  }

  void Init(AlsReader::AlsInitStatus als_reader_status,
            BrightnessMonitor::Status brightness_monitor_status,
            base::Optional<ModelConfig> model_config,
            bool is_trainer_configured = true,
            bool is_personal_curve_valid = true,
            const std::map<std::string, std::string>& params = {}) {
    if (!params.empty()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAutoScreenBrightness, params);
    }

    fake_als_reader_.set_als_init_status(als_reader_status);
    fake_brightness_monitor_.set_status(brightness_monitor_status);
    if (model_config) {
      fake_model_config_loader_.set_model_config(model_config.value());
    }

    SetUpModeller(is_trainer_configured, is_personal_curve_valid);
    thread_bundle_.RunUntilIdle();
  }

 protected:
  void WriteCurveToFile(const MonotoneCubicSpline& curve) {
    const base::FilePath curve_path =
        ModellerImpl::ModellerImpl::GetCurvePathFromProfile(profile_.get());
    CHECK(!curve_path.empty());

    const std::string data = curve.ToString();
    const int bytes_written =
        base::WriteFile(curve_path, data.data(), data.size());
    ASSERT_EQ(bytes_written, static_cast<int>(data.size()))
        << "Wrote " << bytes_written << " byte(s) instead of " << data.size()
        << " to " << curve_path;
  }

  // Returns a valid ModelConfig.
  ModelConfig GetTestModelConfig() {
    ModelConfig model_config;
    model_config.auto_brightness_als_horizon_seconds = 2.0;
    model_config.log_lux = {
        3.69, 4.83, 6.54, 7.68, 8.25, 8.82,
    };
    model_config.brightness = {
        36.14, 47.62, 85.83, 93.27, 93.27, 100,
    };

    model_config.metrics_key = "abc";
    model_config.model_als_horizon_seconds = 3.0;
    return model_config;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::HistogramTester histogram_tester_;

  ui::UserActivityDetector user_activity_detector_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;

  FakeAlsReader fake_als_reader_;
  FakeBrightnessMonitor fake_brightness_monitor_;
  FakeModelConfigLoader fake_model_config_loader_;

  std::unique_ptr<ModellerImpl> modeller_;
  std::unique_ptr<TestObserver> test_observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModellerImplTest);
};

// AlsReader is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, AlsReaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kDisabled, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig());

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// BrightnessMonitor is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kDisabled,
       GetTestModelConfig());

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// ModelConfigLoader has an invalid config, hence Modeller is disabled.
TEST_F(ModellerImplTest, ModelConfigLoaderDisabledOnInit) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       ModelConfig());

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// AlsReader is |kDisabled| on later notification.
TEST_F(ModellerImplTest, AlsReaderDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, GetTestModelConfig());

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kDisabled);
  fake_als_reader_.ReportReaderInitialized();
  thread_bundle_.RunUntilIdle();

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// AlsReader is |kSuccess| on later notification.
TEST_F(ModellerImplTest, AlsReaderEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kInProgress,
       BrightnessMonitor::Status::kSuccess, GetTestModelConfig());

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_als_reader_.ReportReaderInitialized();
  thread_bundle_.RunUntilIdle();

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);
}

// BrightnessMonitor is |kDisabled| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, GetTestModelConfig());

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// BrightnessMonitor is |kSuccess| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorEnabledOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess,
       BrightnessMonitor::Status::kInitializing, GetTestModelConfig());

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  thread_bundle_.RunUntilIdle();

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);
}

// ModelConfigLoader reports an invalid config on later notification.
TEST_F(ModellerImplTest, InvalidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       base::nullopt /* model_config */);

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  // ModelConfig() creates an invalid config.
  DCHECK(!IsValidModelConfig(ModelConfig()));
  fake_model_config_loader_.set_model_config(ModelConfig());
  fake_model_config_loader_.ReportModelConfigLoaded();
  thread_bundle_.RunUntilIdle();
  test_observer_->CheckStatus(true /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);
}

// ModelConfigLoader reports a valid config on later notification.
TEST_F(ModellerImplTest, ValidModelConfigOnNotification) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       base::nullopt /* model_config */);

  test_observer_->CheckStatus(false /* is_model_initialized */,
                              base::nullopt /* global_curve */,
                              base::nullopt /* personal_curve */);

  fake_model_config_loader_.set_model_config(GetTestModelConfig());
  fake_model_config_loader_.ReportModelConfigLoaded();
  thread_bundle_.RunUntilIdle();

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);
}
// There is no saved curve, hence a global curve is created.
TEST_F(ModellerImplTest, NoSavedCurve) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig());

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);
}

// A curve is loaded from disk, this is a personal curve.
TEST_F(ModellerImplTest, CurveLoadedFromProfilePath) {
  const std::vector<double> xs = {0, 10, 20, 40, 60, 80, 90, 100};
  const std::vector<double> ys = {0, 5, 10, 15, 20, 25, 30, 40};
  MonotoneCubicSpline curve(xs, ys);

  WriteCurveToFile(curve);

  thread_bundle_.RunUntilIdle();

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig());

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(), curve);
  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.PersonalCurveValid", true, 1);
}

// A curve is loaded from disk, this is a personal curve. This personal curve
// doesn't satisfy Trainer slope constraint, hence it's ignored and the global
// curve is used instead.
TEST_F(ModellerImplTest, PersonalCurveError) {
  const std::vector<double> xs = {0, 10, 20, 40, 60, 80, 90, 100};
  const std::vector<double> ys = {0, 5, 10, 15, 20, 25, 30, 40};
  MonotoneCubicSpline curve(xs, ys);

  WriteCurveToFile(curve);

  thread_bundle_.RunUntilIdle();

  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig(), true /* is_trainer_configured */,
       false /* is_personal_curve_valid */);

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.PersonalCurveValid", false, 1);
}

// Ambient light values are received. We check average ambient light has been
// calculated from the recent samples only.
TEST_F(ModellerImplTest, OnAmbientLightUpdated) {
  const ModelConfig model_config = GetTestModelConfig();
  // Set a horizon different from model_config.
  const int horizon_in_seconds = 4;
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       model_config, true /* is_trainer_configured */,
       true /* is_personal_curve_valid */,
       {{"model_als_horizon_seconds",
         base::NumberToString(horizon_in_seconds)}});

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);

  EXPECT_EQ(modeller_->GetModelConfigForTesting(), model_config);

  const int first_lux = 1000;
  double running_sum = 0.0;
  for (int i = 0; i < horizon_in_seconds; ++i) {
    thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(1));
    const int lux = i == 0 ? first_lux : i;
    fake_als_reader_.ReportAmbientLightUpdate(lux);
    running_sum += ConvertToLog(lux);
    EXPECT_DOUBLE_EQ(
        modeller_->AverageAmbientForTesting(thread_bundle_.NowTicks()).value(),
        running_sum / (i + 1));
  }

  // Add another one should push the oldest |first_lux| out of the horizon.
  thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(100);
  running_sum = running_sum + ConvertToLog(100) - ConvertToLog(first_lux);
  EXPECT_DOUBLE_EQ(
      modeller_->AverageAmbientForTesting(thread_bundle_.NowTicks()).value(),
      running_sum / horizon_in_seconds);
}

// User brightness changes are received, training example cache reaches
// |max_training_data_points_| to trigger early training. This all happens
// within a small window shorter than |training_delay_|.
TEST_F(ModellerImplTest, OnUserBrightnessChanged) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig(), true /* is_trainer_configured */,
       true /* is_personal_curve_valid */,
       {{"training_delay_in_seconds", base::NumberToString(60)}});

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);

  std::vector<TrainingDataPoint> expected_data;

  for (size_t i = 0; i < modeller_->GetMaxTrainingDataPointsForTesting() - 1;
       ++i) {
    EXPECT_EQ(i, modeller_->NumberTrainingDataPointsForTesting());
    thread_bundle_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
    const base::TimeTicks now = thread_bundle_.NowTicks();
    const int lux = i * 20;
    fake_als_reader_.ReportAmbientLightUpdate(lux);
    const double brightness_old = 10.0 + i;
    const double brightness_new = 20.0 + i;
    modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
    expected_data.push_back({brightness_old, brightness_new,
                             modeller_->AverageAmbientForTesting(now).value(),
                             now});
  }

  // Training should not have started.
  EXPECT_EQ(modeller_->GetMaxTrainingDataPointsForTesting() - 1,
            modeller_->NumberTrainingDataPointsForTesting());

  // Add one more data point to trigger the training early.
  thread_bundle_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  const base::TimeTicks now = thread_bundle_.NowTicks();
  const double brightness_old = 85;
  const double brightness_new = 95;
  modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
  expected_data.push_back({brightness_old, brightness_new,
                           modeller_->AverageAmbientForTesting(now).value(),
                           now});
  thread_bundle_.RunUntilIdle();

  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());

  const base::Optional<MonotoneCubicSpline>& result_curve =
      test_observer_->personal_curve();
  DCHECK(result_curve);

  const MonotoneCubicSpline expected_curve =
      CreateTestCurveFromTrainingData(expected_data);
  EXPECT_EQ(expected_curve, *result_curve);
}

// User activities resets timer used to start training.
TEST_F(ModellerImplTest, MultipleUserActivities) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig(), true /* is_trainer_configured */,
       true /* is_personal_curve_valid */,
       {{"training_delay_in_seconds", base::NumberToString(60)}});

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);

  thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  fake_als_reader_.ReportAmbientLightUpdate(30);
  std::vector<TrainingDataPoint> expected_data;
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_EQ(i, modeller_->NumberTrainingDataPointsForTesting());
    thread_bundle_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
    const base::TimeTicks now = thread_bundle_.NowTicks();
    const int lux = i * 20;
    fake_als_reader_.ReportAmbientLightUpdate(lux);
    const double brightness_old = 10.0 + i;
    const double brightness_new = 20.0 + i;
    modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
    expected_data.push_back({brightness_old, brightness_new,
                             modeller_->AverageAmbientForTesting(now).value(),
                             now});
  }

  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);

  thread_bundle_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);
  // A user activity is received, timer should be reset.
  const ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                                   gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  modeller_->OnUserActivity(&mouse_event);

  thread_bundle_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 3);
  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);

  // Another user event is received.
  modeller_->OnUserActivity(&mouse_event);

  // After |training_delay_|/2, no training has started.
  thread_bundle_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);
  EXPECT_EQ(modeller_->NumberTrainingDataPointsForTesting(), 10u);

  // After another |training_delay_|/2, training is scheduled.
  thread_bundle_.FastForwardBy(modeller_->GetTrainingDelayForTesting() / 2);

  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  const base::Optional<MonotoneCubicSpline>& result_curve =
      test_observer_->personal_curve();
  DCHECK(result_curve);

  const MonotoneCubicSpline expected_curve =
      CreateTestCurveFromTrainingData(expected_data);
  EXPECT_EQ(expected_curve, *result_curve);
}

// Training delay is 0, hence we train as soon as we have 1 data point.
TEST_F(ModellerImplTest, ZeroTrainingDelay) {
  Init(AlsReader::AlsInitStatus::kSuccess, BrightnessMonitor::Status::kSuccess,
       GetTestModelConfig(), true /* is_trainer_configured */,
       true /* is_personal_curve_valid  */,
       {
           {"training_delay_in_seconds", "0"},
       });

  test_observer_->CheckStatus(true /* is_model_initialized */,
                              modeller_->GetGlobalCurveForTesting(),
                              base::nullopt /* personal_curve */);

  fake_als_reader_.ReportAmbientLightUpdate(30);
  const ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                                   gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  modeller_->OnUserActivity(&mouse_event);

  modeller_->OnUserBrightnessChanged(10, 20);
  thread_bundle_.RunUntilIdle();
  EXPECT_EQ(0u, modeller_->NumberTrainingDataPointsForTesting());
  EXPECT_TRUE(test_observer_->trained_curve_received());
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
