// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/random_tree_trainer.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class RandomTreeTest : public testing::Test {
 public:
  RandomTreeTest() : storage_(base::MakeRefCounted<TrainingDataStorage>()) {}

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  RandomTreeTrainer trainer_;
  scoped_refptr<TrainingDataStorage> storage_;
};

TEST_F(RandomTreeTest, EmptyTrainingDataWorks) {
  TrainingData empty(storage_);
  std::unique_ptr<Model> model = trainer_.Train(empty);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(model->PredictDistribution(FeatureVector()), TargetDistribution());
}

TEST_F(RandomTreeTest, UniformTrainingDataWorks) {
  TrainingExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  const int n_examples = 10;
  for (int i = 0; i < n_examples; i++)
    storage_->push_back(example);
  TrainingData training_data(storage_, storage_->begin(), storage_->end());
  std::unique_ptr<Model> model = trainer_.Train(training_data);

  // The tree should produce a distribution for one value (our target), which
  // has |n_examples| counts.
  TargetDistribution distribution =
      model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example.target_value], n_examples);
}

TEST_F(RandomTreeTest, UniformTrainingDataWorksWithCallback) {
  TrainingExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  const int n_examples = 10;
  for (int i = 0; i < n_examples; i++)
    storage_->push_back(example);
  TrainingData training_data(storage_, storage_->begin(), storage_->end());

  // Construct a TrainedModelCB that will store the model locally.
  std::unique_ptr<Model> model;
  TrainedModelCB model_cb = base::BindOnce(
      [](std::unique_ptr<Model>* model_out, std::unique_ptr<Model> model) {
        *model_out = std::move(model);
      },
      &model);

  // Run the trainer.
  RandomTreeTrainer::GetTrainingAlgorithmCB().Run(training_data,
                                                  std::move(model_cb));
  base::RunLoop().RunUntilIdle();

  TargetDistribution distribution =
      model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example.target_value], n_examples);
}

TEST_F(RandomTreeTest, SimpleSeparableTrainingData) {
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(456)}, TargetValue(2));
  storage_->push_back(example_1);
  storage_->push_back(example_2);
  TrainingData training_data(storage_, storage_->begin(), storage_->end());
  std::unique_ptr<Model> model = trainer_.Train(training_data);

  // Each value should have a distribution with one target value with one count.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_1.target_value], 1);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1);
}

TEST_F(RandomTreeTest, ComplexSeparableTrainingData) {
  // Build a four-feature training set that's completely separable, but one
  // needs all four features to do it.
  for (int f1 = 0; f1 < 2; f1++) {
    for (int f2 = 0; f2 < 2; f2++) {
      for (int f3 = 0; f3 < 2; f3++) {
        for (int f4 = 0; f4 < 2; f4++) {
          storage_->push_back(
              TrainingExample({FeatureValue(f1), FeatureValue(f2),
                               FeatureValue(f3), FeatureValue(f4)},
                              TargetValue(f1 * 1 + f2 * 2 + f3 * 4 + f4 * 8)));
        }
      }
    }
  }

  // Add two copies of each example.  Note that we do this after fully
  // constructing |training_data_storage|, since it may realloc.
  TrainingData training_data(storage_);
  for (auto& example : *storage_) {
    training_data.push_back(&example);
    training_data.push_back(&example);
  }

  std::unique_ptr<Model> model = trainer_.Train(training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each example should have a distribution by itself, with two counts.
  for (const TrainingExample* example : training_data) {
    TargetDistribution distribution =
        model->PredictDistribution(example->features);
    EXPECT_EQ(distribution.size(), 1u);
    EXPECT_EQ(distribution[example->target_value], 2);
  }
}

TEST_F(RandomTreeTest, UnseparableTrainingData) {
  TrainingExample example_1({FeatureValue(123)}, TargetValue(1));
  TrainingExample example_2({FeatureValue(123)}, TargetValue(2));
  storage_->push_back(example_1);
  storage_->push_back(example_2);
  TrainingData training_data(storage_, storage_->begin(), storage_->end());
  std::unique_ptr<Model> model = trainer_.Train(training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each value should have a distribution with two targets with one count each.
  TargetDistribution distribution =
      model->PredictDistribution(example_1.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1);
  EXPECT_EQ(distribution[example_2.target_value], 1);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1);
  EXPECT_EQ(distribution[example_2.target_value], 1);
}

}  // namespace learning
}  // namespace media
