// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

TEST(AppLaunchPredictorTest, MrfuAppLaunchPredictor) {
  MrfuAppLaunchPredictor predictor;
  const float decay = MrfuAppLaunchPredictor::decay_coeff_;

  predictor.Train(kTarget1);
  const float score_1 = 1.0f - decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_1))));

  predictor.Train(kTarget1);
  const float score_2 = score_1 + score_1 * decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_2))));

  predictor.Train(kTarget2);
  const float score_3 = score_2 * decay;
  EXPECT_THAT(predictor.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(score_3)),
                                   Pair(kTarget2, FloatEq(score_1))));
}

}  // namespace app_list
