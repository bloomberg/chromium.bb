// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"

namespace app_list {

AppSearchResultRanker::AppSearchResultRanker(Profile* profile) {
  if (!features::IsSearchResultRankerTrainEnabled())
    return;

  if (features::SearchResultRankerPredictorName() == "MrfuAppLaunchPredictor") {
    predictor_ = std::make_unique<MrfuAppLaunchPredictor>();
    load_from_disk_success_ = true;
  } else {
    NOTREACHED();
  }
}

AppSearchResultRanker::~AppSearchResultRanker() = default;

void AppSearchResultRanker::Train(const std::string& app_id) {
  if (load_from_disk_success_ && predictor_) {
    predictor_->Train(app_id);
  }
}

base::flat_map<std::string, float> AppSearchResultRanker::Rank() {
  if (load_from_disk_success_ && features::IsSearchResultRankerInferEnabled() &&
      predictor_) {
    return predictor_->Rank();
  }

  return {};
}

}  // namespace app_list
