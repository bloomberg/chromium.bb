// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "media/learning/common/learning_task.h"
#include "media/learning/impl/distribution_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class DistributionReporterTest : public testing::Test {
 public:
  DistributionReporterTest() {
    task_.name = "TaskName";
    // UMA reporting requires a numeric target.
    task_.target_description.ordering = LearningTask::Ordering::kNumeric;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  LearningTask task_;

  std::unique_ptr<DistributionReporter> reporter_;
};

TEST_F(DistributionReporterTest, DistributionReporterDoesNotCrash) {
  // Make sure that we request some sort of reporting.
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);

  // Observe an average of 2 / 3.
  DistributionReporter::PredictionInfo info;
  info.observed = TargetValue(2.0 / 3.0);
  auto cb = reporter_->GetPredictionCallback(info);

  TargetHistogram predicted;
  const TargetValue Zero(0);
  const TargetValue One(1);

  // Predict an average of 5 / 9.
  predicted[Zero] = 40;
  predicted[One] = 50;
  std::move(cb).Run(predicted);
}

TEST_F(DistributionReporterTest, DistributionReporterMustBeRequested) {
  // Make sure that we don't get a reporter if we don't request any reporting.
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_EQ(reporter_, nullptr);
}

TEST_F(DistributionReporterTest,
       DistributionReporterHackyConfusionMatrixNeedsRegression) {
  // Hacky confusion matrix reporting only works with regression.
  task_.target_description.ordering = LearningTask::Ordering::kUnordered;
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_EQ(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesAggregateReporter) {
  task_.uma_hacky_aggregate_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesByTrainingWeightReporter) {
  task_.uma_hacky_by_training_weight_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

TEST_F(DistributionReporterTest, ProvidesByFeatureSubsetReporter) {
  task_.uma_hacky_by_feature_subset_confusion_matrix = true;
  reporter_ = DistributionReporter::Create(task_);
  EXPECT_NE(reporter_, nullptr);
}

}  // namespace learning
}  // namespace media
