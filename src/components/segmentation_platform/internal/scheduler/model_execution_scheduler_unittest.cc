// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler_impl.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_storage_config.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/database/signal_storage_config.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::SaveArg;

namespace segmentation_platform {
using SignalType = proto::SignalType;
using SignalIdentifier = std::pair<uint64_t, SignalType>;
using CleanupItem = std::tuple<uint64_t, SignalType, base::Time>;

namespace {
constexpr auto kTestOptimizationTarget =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB;
constexpr auto kTestOptimizationTarget2 =
    OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY;
}  // namespace

class MockModelExecutionObserver : public ModelExecutionScheduler::Observer {
 public:
  MockModelExecutionObserver() = default;
  MOCK_METHOD(void, OnModelExecutionCompleted, (OptimizationTarget));
};

class MockModelExecutionManager : public ModelExecutionManager {
 public:
  MockModelExecutionManager() = default;
  MOCK_METHOD(void, ExecuteModel, (OptimizationTarget, ModelExecutionCallback));
};

class ModelExecutionSchedulerTest : public testing::Test {
 public:
  ModelExecutionSchedulerTest() = default;
  ~ModelExecutionSchedulerTest() override = default;

  void SetUp() override {
    clock_.SetNow(base::Time::Now());
    std::vector<ModelExecutionScheduler::Observer*> observers = {&observer1_,
                                                                 &observer2_};
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    base::flat_set<OptimizationTarget> segment_ids;
    segment_ids.insert(kTestOptimizationTarget);
    model_execution_scheduler_ = std::make_unique<ModelExecutionSchedulerImpl>(
        std::move(observers), segment_database_.get(), &signal_storage_config_,
        &model_execution_manager_, segment_ids, &clock_,
        PlatformOptions::CreateDefault());
  }

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  MockModelExecutionObserver observer1_;
  MockModelExecutionObserver observer2_;
  MockSignalStorageConfig signal_storage_config_;
  MockModelExecutionManager model_execution_manager_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<ModelExecutionScheduler> model_execution_scheduler_;
};

TEST_F(ModelExecutionSchedulerTest, OnNewModelInfoReady) {
  auto* segment_info =
      segment_database_->FindOrCreateSegment(kTestOptimizationTarget);
  segment_info->set_segment_id(kTestOptimizationTarget);
  auto* metadata = segment_info->mutable_model_metadata();
  metadata->set_result_time_to_live(1);
  metadata->set_time_unit(proto::TimeUnit::DAY);

  // If the metadata DOES NOT meet the signal requirement, we SHOULD NOT try to
  // execute the model.
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(0);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(false));
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If the metadata DOES meet the signal requirement, and we have no old,
  // PredictionResult we SHOULD try to execute the model.
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(1);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillOnce(Return(true));
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we just got a new result, we SHOULD NOT try to execute the model.
  auto* prediction_result = segment_info->mutable_prediction_result();
  prediction_result->set_result(0.9);
  prediction_result->set_timestamp_us(
      clock_.Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(0);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));  // Ensure this part has positive result.
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we have a non-fresh, but not expired result, we SHOULD NOT try to
  // execute the model.
  base::Time not_expired_timestamp =
      clock_.Now() - base::Days(1) + base::Hours(1);
  prediction_result->set_result(0.9);
  prediction_result->set_timestamp_us(
      not_expired_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(0);
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);

  // If we have an expired result, we SHOULD try to execute the model.
  base::Time just_expired_timestamp =
      clock_.Now() - base::Days(1) - base::Hours(1);
  prediction_result->set_result(0.9);
  prediction_result->set_timestamp_us(
      just_expired_timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(1);
  model_execution_scheduler_->OnNewModelInfoReady(*segment_info);
}

TEST_F(ModelExecutionSchedulerTest, RequestModelExecutionForEligibleSegments) {
  segment_database_->FindOrCreateSegment(kTestOptimizationTarget);
  segment_database_->FindOrCreateSegment(kTestOptimizationTarget2);

  // TODO(shaktisahu): Add tests for expired segments, freshly computed segments
  // etc.

  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget, _))
      .Times(1);
  EXPECT_CALL(signal_storage_config_, MeetsSignalCollectionRequirement(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(model_execution_manager_,
              ExecuteModel(kTestOptimizationTarget2, _))
      .Times(0);
  // TODO(shaktisahu): Add test when the signal collection returns false.

  model_execution_scheduler_->RequestModelExecutionForEligibleSegments(true);
}

TEST_F(ModelExecutionSchedulerTest, OnModelExecutionCompleted) {
  proto::SegmentInfo* segment_info =
      segment_database_->FindOrCreateSegment(kTestOptimizationTarget);

  // TODO(shaktisahu): Add tests for model failure.
  EXPECT_CALL(observer2_, OnModelExecutionCompleted(kTestOptimizationTarget))
      .Times(1);
  EXPECT_CALL(observer1_, OnModelExecutionCompleted(kTestOptimizationTarget))
      .Times(1);
  float score = 0.4;
  model_execution_scheduler_->OnModelExecutionCompleted(
      kTestOptimizationTarget,
      std::make_pair(score, ModelExecutionStatus::kSuccess));

  // Verify that the results are written to the DB.
  segment_info =
      segment_database_->FindOrCreateSegment(kTestOptimizationTarget);
  ASSERT_TRUE(segment_info->has_prediction_result());
  ASSERT_EQ(score, segment_info->prediction_result().result());
}

}  // namespace segmentation_platform
