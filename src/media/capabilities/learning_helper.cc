// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capabilities/learning_helper.h"

#include "base/task/post_task.h"
#include "media/learning/common/feature_library.h"
#include "media/learning/common/learning_task.h"

namespace media {

using learning::FeatureLibrary;
using learning::FeatureProviderFactoryCB;
using learning::FeatureValue;
using learning::LabelledExample;
using learning::LearningSessionImpl;
using learning::LearningTask;
using learning::LearningTaskController;
using learning::ObservationCompletion;
using learning::SequenceBoundFeatureProvider;
using learning::TargetValue;

// Remember that these are used to construct UMA histogram names!  Be sure to
// update histograms.xml if you change them!
// Dropped frame ratio, default features, regression tree.
const char* const kDroppedFrameRatioBaseTreeTaskName = "BaseTree";
// Dropped frame ratio, default+FeatureLibrary features, regression tree.
const char* const kDroppedFrameRatioEnhancedTreeTaskName = "EnhancedTree";
// Dropped frame ratio, default+FeatureLibrary features, regression tree,
// examples are unweighted.
const char* const kDroppedFrameRatioEnhancedUnweightedTreeTaskName =
    "EnhancedUnweightedTree";
// Binary smoothness, default+FeatureLibrary features, regression tree,
// examples are unweighted.
const char* const kBinarySmoothnessEnhancedUnweightedTreeTaskName =
    "BinarySmoothnessTree";
// Dropped frame ratio, default features, lookup table.
const char* const kDroppedFrameRatioBaseTableTaskName = "BaseTable";

// Threshold for the dropped frame to total frame ratio, at which we'll decide
// that the playback was not smooth.
constexpr double kSmoothnessThreshold = 0.1;

LearningHelper::LearningHelper(FeatureProviderFactoryCB feature_factory) {
  // Create the LearningSession on a background task runner.  In the future,
  // it's likely that the session will live on the main thread, and handle
  // delegation of LearningTaskControllers to other threads.  However, for now,
  // do it here.
  learning_session_ = std::make_unique<LearningSessionImpl>(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  // Register a few learning tasks.
  //
  // We only do this here since we own the session.  Normally, whatever creates
  // the session would register all the learning tasks.
  LearningTask dropped_frame_task(
      kDroppedFrameRatioBaseTableTaskName, LearningTask::Model::kLookupTable,
      {
          {"codec_profile",
           ::media::learning::LearningTask::Ordering::kUnordered},
          {"width", ::media::learning::LearningTask::Ordering::kNumeric},
          {"height", ::media::learning::LearningTask::Ordering::kNumeric},
          {"frame_rate", ::media::learning::LearningTask::Ordering::kNumeric},
      },
      LearningTask::ValueDescription(
          {"dropped_ratio", LearningTask::Ordering::kNumeric}));

  // Report results hackily both in aggregate and by training data weight.
  dropped_frame_task.smoothness_threshold = kSmoothnessThreshold;
  dropped_frame_task.uma_hacky_aggregate_confusion_matrix = true;
  dropped_frame_task.uma_hacky_by_training_weight_confusion_matrix = true;

  // Pick a max reporting weight that represents the total number of frames.
  // This will record in bucket [0, 4999], [5000, 9999], etc.  Unlike the
  // existing mcap thresholds, these are not per-bucket.  That's why they're 10x
  // higher than the per-bucket thresholds we're using there.  Mcap allows on
  // the order of 2,500 frames in each of {resolution X fps X codec} buckets,
  // while the reported training weight here would be total for the whole set.
  // So, we multiply by about 20 to approximate the number of buckets to keep
  // it about the same as the size of the cross product.
  const double weighted_reporting_max = 49999.;
  dropped_frame_task.max_reporting_weight = weighted_reporting_max;

  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());
  base_table_controller_ =
      learning_session_->GetController(dropped_frame_task.name);

  // Modify the task to use ExtraTrees.
  dropped_frame_task.name = kDroppedFrameRatioBaseTreeTaskName;
  dropped_frame_task.model = LearningTask::Model::kExtraTrees;
  learning_session_->RegisterTask(dropped_frame_task,
                                  SequenceBoundFeatureProvider());
  base_tree_controller_ =
      learning_session_->GetController(dropped_frame_task.name);

  // Add common features, if we have a factory.
  if (feature_factory) {
    dropped_frame_task.name = kDroppedFrameRatioEnhancedTreeTaskName;
    dropped_frame_task.feature_descriptions.push_back(
        {"origin", ::media::learning::LearningTask::Ordering::kUnordered});
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::NetworkType());
    dropped_frame_task.feature_descriptions.push_back(
        FeatureLibrary::BatteryPower());
    learning_session_->RegisterTask(dropped_frame_task,
                                    feature_factory.Run(dropped_frame_task));
    enhanced_tree_controller_ =
        learning_session_->GetController(dropped_frame_task.name);

    // Duplicate the task with a new name and UMA histogram.  We'll add
    // unweighted examples to it to see which one does better.
    dropped_frame_task.name = kDroppedFrameRatioEnhancedUnweightedTreeTaskName;
    // Adjust the reporting weight since we'll have 100 or fewer examples.
    dropped_frame_task.max_reporting_weight = 99.;
    learning_session_->RegisterTask(dropped_frame_task,
                                    feature_factory.Run(dropped_frame_task));
    unweighted_tree_controller_ =
        learning_session_->GetController(dropped_frame_task.name);

    // Set up the binary smoothness task.  This has a nominal target, with
    // "smooth" as 0, and "not smooth" as 1.  This is so that the low numbers
    // are still smooth, and the hight numbers are still not smooth.  It makes
    // reporting the same for both.
    dropped_frame_task.name = kBinarySmoothnessEnhancedUnweightedTreeTaskName;
    /* TODO(liberato): DistributionReporter only supports regression, so we
       leave it as kNumeric.  Since we only add 0,1 as targets, it's probably
       fairly close to the same thing.
    dropped_frame_task.target_description = {
        "is_smooth", ::media::learning::LearningTask::Ordering::kUnordered};
    */
    // We'll threshold the ratio when figuring out the binary label, so we just
    // want to pick the majority.  Note that I have no idea if this is actually
    // the best threshold, but it seems like a good place to start.
    dropped_frame_task.smoothness_threshold = 0.5;
    dropped_frame_task.max_reporting_weight = weighted_reporting_max;
    learning_session_->RegisterTask(dropped_frame_task,
                                    feature_factory.Run(dropped_frame_task));
    binary_tree_controller_ =
        learning_session_->GetController(dropped_frame_task.name);
  }
}

LearningHelper::~LearningHelper() = default;

void LearningHelper::AppendStats(
    const VideoDecodeStatsDB::VideoDescKey& video_key,
    learning::FeatureValue origin,
    const VideoDecodeStatsDB::DecodeStatsEntry& new_stats) {
  // If no frames were recorded, then do nothing.
  if (new_stats.frames_decoded == 0)
    return;

  // Sanity.
  if (new_stats.frames_dropped > new_stats.frames_decoded)
    return;

  // Add a training example for |new_stats|.
  LabelledExample example;

  // Extract features from |video_key|.
  example.features.push_back(FeatureValue(video_key.codec_profile));
  example.features.push_back(FeatureValue(video_key.size.width()));
  example.features.push_back(FeatureValue(video_key.size.height()));
  example.features.push_back(FeatureValue(video_key.frame_rate));

  // Record the ratio of dropped frames to non-dropped frames.  Weight this
  // example by the total number of frames, since we want to predict the
  // aggregate dropped frames ratio.  That lets us compare with the current
  // implementation directly.
  //
  // It's also not clear that we want to do this; we might want to weight each
  // playback equally and predict the dropped frame ratio.  For example, if
  // there is a dependence on video length, then it's unclear that weighting
  // the examples is the right thing to do.
  example.target_value = TargetValue(
      static_cast<double>(new_stats.frames_dropped) / new_stats.frames_decoded);
  example.weight = new_stats.frames_decoded;

  // Add this example to all tasks.
  AddExample(base_table_controller_.get(), example);
  AddExample(base_tree_controller_.get(), example);
  if (enhanced_tree_controller_) {
    example.features.push_back(origin);
    AddExample(enhanced_tree_controller_.get(), example);

    // Also add to the unweighted model.
    example.weight = 1u;
    AddExample(unweighted_tree_controller_.get(), example);

    // Threshold the target to 0 for "smooth", and 1 for "not smooth".
    example.target_value =
        TargetValue(example.target_value.value() > kSmoothnessThreshold);
    AddExample(binary_tree_controller_.get(), example);
  }
}

void LearningHelper::AddExample(LearningTaskController* controller,
                                const LabelledExample& example) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  controller->BeginObservation(id, example.features);
  controller->CompleteObservation(
      id, ObservationCompletion(example.target_value, example.weight));
}

}  // namespace media
