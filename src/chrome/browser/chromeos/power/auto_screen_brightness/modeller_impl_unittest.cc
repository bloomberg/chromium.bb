// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/modeller_impl.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_als_reader.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_brightness_monitor.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

void CompareBrightnessCurve(const BrightnessCurve& expected,
                            const BrightnessCurve& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_DOUBLE_EQ(expected[i].first, actual[i].first);
    EXPECT_DOUBLE_EQ(expected[i].second, actual[i].second);
  }
}

std::pair<double, double> DataToCurvePoint(const TrainingDataPoint& data) {
  return std::make_pair(data.ambient_lux,
                        (data.brightness_old + data.brightness_new) / 2);
}

// Testing trainer.
class FakeTrainer : public Trainer {
 public:
  FakeTrainer() = default;
  ~FakeTrainer() override = default;

  // Trainer overrides:
  BrightnessCurve Train(const BrightnessCurve& /* curve */,
                        const std::vector<TrainingDataPoint>& data) override {
    BrightnessCurve result_curve;
    for (const auto& training_data_point : data) {
      result_curve.push_back(DataToCurvePoint(training_data_point));
    }
    return result_curve;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeTrainer);
};

class TestObserver : public Modeller::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override = default;

  // Modeller::Observer overrides:
  void OnModelTrained(const BrightnessCurve& brightness_curve) override {
    brightness_curve_ = base::Optional<BrightnessCurve>(brightness_curve);
  }

  void OnModelInitialized(const Modeller::Status model_status,
                          const BrightnessCurve& brightness_curve) override {
    model_status_ = base::Optional<Modeller::Status>(model_status);
    brightness_curve_ = base::Optional<BrightnessCurve>(brightness_curve);
  }

  Modeller::Status model_status() const {
    DCHECK(model_status_.has_value());
    return model_status_.value();
  }

  BrightnessCurve brightness_curve() const {
    DCHECK(brightness_curve_.has_value());
    return brightness_curve_.value();
  }

  bool HasModelStatus() { return model_status_.has_value(); }
  bool HasBrightnessCurve() { return brightness_curve_.has_value(); }

 private:
  base::Optional<Modeller::Status> model_status_;
  base::Optional<BrightnessCurve> brightness_curve_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class ModellerImplTest : public testing::Test {
 public:
  ModellerImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        thread_bundle_(content::TestBrowserThreadBundle::PLAIN_MAINLOOP) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("testuser@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestProfile"));
    profile_ = profile_builder.Build();
  }

  ~ModellerImplTest() override {
    base::TaskScheduler::GetInstance()->FlushForTesting();
  }

  void SetUpModeller() {
    modeller_ = ModellerImpl::CreateForTesting(
        profile_.get(), &fake_als_reader_, &fake_brightness_monitor_,
        &user_activity_detector_, std::make_unique<FakeTrainer>(),
        base::SequencedTaskRunnerHandle::Get(),
        scoped_task_environment_.GetMockTickClock());

    test_observer_ = std::make_unique<TestObserver>();
    modeller_->AddObserver(test_observer_.get());
  }

 protected:
  void WriteCurveToFile(const BrightnessCurve& curve) {
    const base::FilePath curve_path =
        modeller_->GetCurvePathForTesting(profile_.get());
    CHECK(!curve_path.empty());

    const std::string& data = CurveToString(curve);
    const int bytes_written =
        base::WriteFile(curve_path, data.data(), data.size());
    ASSERT_EQ(bytes_written, static_cast<int>(data.size()))
        << "Wrote " << bytes_written << " byte(s) instead of " << data.size()
        << " to " << curve_path;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle thread_bundle_;

  ui::UserActivityDetector user_activity_detector_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;

  FakeAlsReader fake_als_reader_;
  FakeBrightnessMonitor fake_brightness_monitor_;

  std::unique_ptr<ModellerImpl> modeller_;
  std::unique_ptr<TestObserver> test_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ModellerImplTest);
};

// AlsReader is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, AlsReaderDisabledOnInit) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kDisabled);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(Modeller::Status::kDisabled, test_observer_->model_status());
}

// BrightnessMonitor is |kDisabled| when Modeller is created.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnInit) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(Modeller::Status::kDisabled, test_observer_->model_status());
}

// AlsReader is |kDisabled| on later notification.
TEST_F(ModellerImplTest, AlsReaderDisabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kInProgress);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_observer_->HasModelStatus());
  EXPECT_FALSE(test_observer_->HasBrightnessCurve());

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kDisabled);
  fake_als_reader_.ReportReaderInitialized();
  EXPECT_EQ(Modeller::Status::kDisabled, test_observer_->model_status());
}

// AlsReader is |kSuccess| on later notification.
TEST_F(ModellerImplTest, AlsReaderEnabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kInProgress);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_observer_->HasModelStatus());
  EXPECT_FALSE(test_observer_->HasBrightnessCurve());

  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_als_reader_.ReportReaderInitialized();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());
  EXPECT_TRUE(test_observer_->brightness_curve().empty());
}

// BrightnessMonitor is |kDisabled| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorDisabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kInitializing);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_observer_->HasModelStatus());
  EXPECT_FALSE(test_observer_->HasBrightnessCurve());

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kDisabled);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  EXPECT_EQ(Modeller::Status::kDisabled, test_observer_->model_status());
}

// BrightnessMonitor is |kSuccess| on later notification.
TEST_F(ModellerImplTest, BrightnessMonitorEnabledOnNotification) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kInitializing);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_FALSE(test_observer_->HasModelStatus());
  EXPECT_FALSE(test_observer_->HasBrightnessCurve());

  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  fake_brightness_monitor_.ReportBrightnessMonitorInitialized();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());
  EXPECT_TRUE(test_observer_->brightness_curve().empty());
}

// There is no saved curve, hence a global curve is created.
TEST_F(ModellerImplTest, NoSavedCurve) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());
  EXPECT_TRUE(test_observer_->brightness_curve().empty());
}

// A curve is loaded from disk, this is a personal curve.
TEST_F(ModellerImplTest, CurveLoadedFromProfilePath) {
  const BrightnessCurve curve = {{10, 15}, {20, 25}, {30, 35}, {40, 45},
                                 {50, 55}, {60, 65}, {70, 75}, {80, 85}};
  WriteCurveToFile(curve);

  scoped_task_environment_.RunUntilIdle();
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(Modeller::Status::kPersonal, test_observer_->model_status());
  EXPECT_EQ(curve, test_observer_->brightness_curve());
}

// Ambient light values are received. We check average ambient light has been
// calculated from the past |kNumberAmbientValuesToTrack| samples only.
TEST_F(ModellerImplTest, OnAmbientLightUpdated) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());

  const int first_lux = 1000;
  double running_sum = first_lux;
  fake_als_reader_.ReportAmbientLightUpdate(first_lux);
  for (int i = 1; i < ModellerImpl::kNumberAmbientValuesToTrack; ++i) {
    const int lux = i;
    fake_als_reader_.ReportAmbientLightUpdate(lux);
    running_sum += lux;
    EXPECT_DOUBLE_EQ(running_sum / (i + 1),
                     modeller_->AverageAmbientForTesting());
  }

  // Add another one should push the oldest |first_lux| out of the horizon.
  fake_als_reader_.ReportAmbientLightUpdate(100);
  running_sum = running_sum + 100 - first_lux;
  EXPECT_DOUBLE_EQ(running_sum / ModellerImpl::kNumberAmbientValuesToTrack,
                   modeller_->AverageAmbientForTesting());
}

// User brightness changes are received, training example cache reaches
// |kMaxTrainingDataPoints| to trigger early training. This all happens within a
// small window shorter than |kTrainingDelay|.
TEST_F(ModellerImplTest, OnUserBrightnessChanged) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());

  std::vector<TrainingDataPoint> expected_data;
  for (int i = 0; i < ModellerImpl::kMaxTrainingDataPoints - 1; ++i) {
    EXPECT_EQ(i, modeller_->NumberTrainingDataPointsForTesting());
    scoped_task_environment_.FastForwardBy(
        base::TimeDelta::FromMilliseconds(1));
    const base::TimeTicks now =
        scoped_task_environment_.GetMockTickClock()->NowTicks();
    const int lux = i * 20;
    fake_als_reader_.ReportAmbientLightUpdate(lux);
    const double brightness_old = 10.0 + i;
    const double brightness_new = 20.0 + i;
    modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
    expected_data.push_back({brightness_old, brightness_new,
                             modeller_->AverageAmbientForTesting(), now});
  }

  // Training should not have started.
  EXPECT_EQ(ModellerImpl::kMaxTrainingDataPoints - 1,
            modeller_->NumberTrainingDataPointsForTesting());

  // Add one more data point to trigger the training early.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  const base::TimeTicks now =
      scoped_task_environment_.GetMockTickClock()->NowTicks();
  const double brightness_old = 85;
  const double brightness_new = 95;
  modeller_->OnUserBrightnessChanged(brightness_old, brightness_new);
  expected_data.push_back({brightness_old, brightness_new,
                           modeller_->AverageAmbientForTesting(), now});
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(0, modeller_->NumberTrainingDataPointsForTesting());
  const BrightnessCurve result_curve = test_observer_->brightness_curve();
  BrightnessCurve expected_curve;
  for (auto& training_data_point : expected_data) {
    expected_curve.push_back(DataToCurvePoint(training_data_point));
  }
  CompareBrightnessCurve(expected_curve, result_curve);
}

// User activities resets timer used to start training.
TEST_F(ModellerImplTest, MultipleUserActivities) {
  fake_als_reader_.set_als_init_status(AlsReader::AlsInitStatus::kSuccess);
  fake_brightness_monitor_.set_status(BrightnessMonitor::Status::kSuccess);
  SetUpModeller();
  scoped_task_environment_.RunUntilIdle();
  ASSERT_EQ(Modeller::Status::kGlobal, test_observer_->model_status());

  fake_als_reader_.ReportAmbientLightUpdate(30);
  modeller_->OnUserBrightnessChanged(10, 20);
  const TrainingDataPoint expected_data = {
      10, 20, 30, scoped_task_environment_.GetMockTickClock()->NowTicks()};
  EXPECT_EQ(1, modeller_->NumberTrainingDataPointsForTesting());

  scoped_task_environment_.FastForwardBy(ModellerImpl::kTrainingDelay -
                                         base::TimeDelta::FromSeconds(10));
  // A user activity is received, timer should be reset.
  const ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(0, 0),
                                   gfx::Point(0, 0), base::TimeTicks(), 0, 0);
  modeller_->OnUserActivity(&mouse_event);

  scoped_task_environment_.FastForwardBy(ModellerImpl::kTrainingDelay -
                                         base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(1, modeller_->NumberTrainingDataPointsForTesting());

  // Another user event is received.
  modeller_->OnUserActivity(&mouse_event);

  // After |kTrainingDelay| - 2 seconds, no training has started.
  scoped_task_environment_.FastForwardBy(ModellerImpl::kTrainingDelay -
                                         base::TimeDelta::FromSeconds(2));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(1, modeller_->NumberTrainingDataPointsForTesting());

  // After another 2 seconds, training is scheduled.
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(2));
  scoped_task_environment_.RunUntilIdle();

  EXPECT_EQ(0, modeller_->NumberTrainingDataPointsForTesting());
  const BrightnessCurve result_curve = test_observer_->brightness_curve();
  BrightnessCurve expected_curve = {DataToCurvePoint(expected_data)};
  CompareBrightnessCurve(expected_curve, result_curve);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
