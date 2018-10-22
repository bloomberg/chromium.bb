// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::Pair;
using testing::FloatEq;

namespace app_list {

namespace {

constexpr char kTarget1[] = "Target1";
constexpr char kTarget2[] = "Target2";

}  // namespace

TEST(AppSearchResultRankerTest, TrainAndInfer) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableSearchResultRankerTrain,
       features::kEnableSearchResultRankerInfer},
      {});

  AppSearchResultRanker ranker(nullptr);
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);

  const float decay = MrfuAppLaunchPredictor::decay_coeff_;

  EXPECT_THAT(
      ranker.Rank(),
      UnorderedElementsAre(Pair(kTarget1, FloatEq((1.0f - decay) * decay)),
                           Pair(kTarget2, FloatEq(1.0f - decay))));
}

TEST(AppSearchResultRankerTest, ReturnEmptyIfInferIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableSearchResultRankerTrain},
      {features::kEnableSearchResultRankerInfer});

  AppSearchResultRanker ranker(nullptr);
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);

  EXPECT_TRUE(ranker.Rank().empty());
}

TEST(AppSearchResultRankerTest, ReturnEmptyIfTrainIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableSearchResultRankerTrain},
      {features::kEnableSearchResultRankerInfer});

  AppSearchResultRanker ranker(nullptr);
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);

  EXPECT_TRUE(ranker.Rank().empty());
}

}  // namespace app_list
