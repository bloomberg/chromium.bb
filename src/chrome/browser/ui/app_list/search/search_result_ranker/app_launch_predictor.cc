// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"

#include <cmath>

#include "base/logging.h"

namespace app_list {

MrfuAppLaunchPredictor::MrfuAppLaunchPredictor() = default;
MrfuAppLaunchPredictor::~MrfuAppLaunchPredictor() = default;

// Updates the score for this |app_id|.
void MrfuAppLaunchPredictor::Train(const std::string& app_id) {
  ++num_of_trains_;
  Score& score = scores_[app_id];
  UpdateScore(&score);
  score.last_score += 1.0f - decay_coeff_;
}

// Updates all scores and return app_id to score map.
base::flat_map<std::string, float> MrfuAppLaunchPredictor::Rank() {
  base::flat_map<std::string, float> output;
  for (auto& pair : scores_) {
    UpdateScore(&pair.second);
    output[pair.first] = pair.second.last_score;
  }
  return output;
}

// Updates last_score and last_update_timestamp.
void MrfuAppLaunchPredictor::UpdateScore(Score* score) {
  const int trains_since_last_time =
      num_of_trains_ - score->num_of_trains_at_last_update;
  if (trains_since_last_time > 0) {
    score->last_score *= std::pow(decay_coeff_, trains_since_last_time);
    score->num_of_trains_at_last_update = num_of_trains_;
  }
}

}  // namespace app_list
