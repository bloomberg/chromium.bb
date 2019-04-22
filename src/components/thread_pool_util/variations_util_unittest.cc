// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/thread_pool_util/variations_util.h"

#include <map>
#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/task/thread_pool/scheduler_worker_params.h"
#include "base/task/thread_pool/scheduler_worker_pool_params.h"
#include "components/variations/variations_params_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace thread_pool_util {

namespace {

class ThreadPoolUtilVariationsUtilTest : public testing::Test {
 protected:
  ThreadPoolUtilVariationsUtilTest() = default;

  void SetVariationParams(
      const std::map<std::string, std::string>& variation_params) {
    std::set<std::string> features;
    features.insert(kRendererSchedulerInitParams.name);
    variation_params_manager_.SetVariationParamsWithFeatureAssociations(
        "DummyTrial", variation_params, features);
  }

 private:
  variations::testing::VariationParamsManager variation_params_manager_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolUtilVariationsUtilTest);
};

}  // namespace

TEST_F(ThreadPoolUtilVariationsUtilTest, OrderingParams5) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0;42";
  variation_params["Foreground"] = "4;4;1;0;62";
  SetVariationParams(variation_params);

  auto init_params = GetThreadPoolInitParams(kRendererSchedulerInitParams);
  ASSERT_TRUE(init_params);

  EXPECT_EQ(1, init_params->background_worker_pool_params.max_tasks());
  EXPECT_EQ(
      base::TimeDelta::FromMilliseconds(42),
      init_params->background_worker_pool_params.suggested_reclaim_time());
  EXPECT_EQ(
      base::SchedulerBackwardCompatibility::DISABLED,
      init_params->background_worker_pool_params.backward_compatibility());

  EXPECT_EQ(4, init_params->foreground_worker_pool_params.max_tasks());
  EXPECT_EQ(
      base::TimeDelta::FromMilliseconds(62),
      init_params->foreground_worker_pool_params.suggested_reclaim_time());
  EXPECT_EQ(
      base::SchedulerBackwardCompatibility::DISABLED,
      init_params->foreground_worker_pool_params.backward_compatibility());
}

TEST_F(ThreadPoolUtilVariationsUtilTest, NoData) {
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

TEST_F(ThreadPoolUtilVariationsUtilTest, IncompleteParameters) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0";
  variation_params["Foreground"] = "4;4;1;0";
  SetVariationParams(variation_params);
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

TEST_F(ThreadPoolUtilVariationsUtilTest, InvalidParametersFormat) {
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "a;b;c;d;e";
  variation_params["Foreground"] = "a;b;c;d;e";
  SetVariationParams(variation_params);
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

TEST_F(ThreadPoolUtilVariationsUtilTest, ZeroMaxThreads) {
  // The Background pool has a maximum number of threads equal to zero, which is
  // invalid.
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "0;0;0;0;0";
  variation_params["Foreground"] = "4;4;1;0;62";
  SetVariationParams(variation_params);
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

TEST_F(ThreadPoolUtilVariationsUtilTest, NegativeMaxThreads) {
  // The Background pool has a negative maximum number of threads, which is
  // invalid.
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "-5;-5;0;0;0";
  variation_params["Foreground"] = "4;4;1;0;62";
  SetVariationParams(variation_params);
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

TEST_F(ThreadPoolUtilVariationsUtilTest, NegativeSuggestedReclaimTime) {
  // The Background pool has a negative suggested reclaim time, which is
  // invalid.
  std::map<std::string, std::string> variation_params;
  variation_params["Background"] = "1;1;1;0;-5";
  variation_params["Foreground"] = "4;4;1;0;62";
  SetVariationParams(variation_params);
  EXPECT_FALSE(GetThreadPoolInitParams(kRendererSchedulerInitParams));
}

}  // namespace thread_pool_util
