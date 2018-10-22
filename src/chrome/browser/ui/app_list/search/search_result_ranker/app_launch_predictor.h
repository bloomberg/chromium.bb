// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace app_list {

// AppLaunchPredictor is the interface implemented by all predictors. It defines
// two basic public functions Train and Rank for training and inferencing.
class AppLaunchPredictor {
 public:
  virtual ~AppLaunchPredictor() = default;
  // Trains on the |app_id| and (possibly) updates its internal representation.
  virtual void Train(const std::string& app_id) = 0;
  // Returns a map of app_id and score.
  //  (1) Higher score means more relevant.
  //  (2) Only returns a subset of app_ids seen by this predictor.
  //  (3) The returned scores should be in range [0.0, 1.0] for
  //      AppSearchProvider to handle.
  virtual base::flat_map<std::string, float> Rank() = 0;
};

// MrfuAppLaunchPredictor is a simple AppLaunchPredictor that balances MRU (most
// recently used) and MFU (most frequently used). It is adopted from LRFU cpu
// cache algorithm.
class MrfuAppLaunchPredictor : public AppLaunchPredictor {
 public:
  MrfuAppLaunchPredictor();
  ~MrfuAppLaunchPredictor() override;

  // AppLaunchPredictor:
  void Train(const std::string& app_id) override;
  base::flat_map<std::string, float> Rank() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AppLaunchPredictorTest, MrfuAppLaunchPredictor);
  FRIEND_TEST_ALL_PREFIXES(AppSearchResultRankerTest, TrainAndInfer);

  // Records last updates of the Score for an app.
  struct Score {
    int32_t num_of_trains_at_last_update = 0;
    float last_score = 0.0f;
  };

  // Updates the Score to now.
  void UpdateScore(Score* score);

  // Controls how much the score decays for each Train() call.
  // This decay_coeff_ should be within [0.5f, 1.0f]. Setting it as 0.5f means
  // MRU; setting as 1.0f means MFU;
  // TODO(https://crbug.com/871674):
  // (1) Set a better initial value based on real user data.
  // (2) Dynamically change this coeff instead of setting it as constant.
  static constexpr float decay_coeff_ = 0.8f;
  // Map from app_id to its Score.
  base::flat_map<std::string, Score> scores_;
  // Increment 1 for each Train() call.
  int32_t num_of_trains_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MrfuAppLaunchPredictor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_PREDICTOR_H_
