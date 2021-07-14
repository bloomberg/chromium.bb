// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/model_execution_manager_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/test_segment_info_database.h"
#include "components/segmentation_platform/internal/execution/feature_aggregator_impl.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {
class ModelExecutionManagerFactoryTest : public testing::Test {
 public:
  ModelExecutionManagerFactoryTest() = default;
  ~ModelExecutionManagerFactoryTest() override = default;

  void SetUp() override {
    optimization_guide_model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();
    segment_database_ = std::make_unique<test::TestSegmentInfoDatabase>();
    feature_aggregator_ = std::make_unique<FeatureAggregatorImpl>();
    test_clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    // Allow for the SegmentationModelExecutor owned by SegmentationModelHandler
    // to be destroyed.
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      optimization_guide_model_provider_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<test::TestSegmentInfoDatabase> segment_database_;
  std::unique_ptr<SignalDatabase> signal_database_;
  std::unique_ptr<FeatureAggregatorImpl> feature_aggregator_;
};

TEST_F(ModelExecutionManagerFactoryTest, CreateModelExecutionManager) {
  auto model_execution_manager = CreateModelExecutionManager(
      optimization_guide_model_provider_.get(),
      task_environment_.GetMainThreadTaskRunner(),
      {OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB},
      &test_clock_, segment_database_.get(), signal_database_.get(),
      std::move(feature_aggregator_), base::DoNothing());
  // This should work regardless of whether a DummyModelExecutionManager or
  // ModelExecutionManagerImpl is returned.
  CHECK(model_execution_manager);
}

}  // namespace segmentation_platform
