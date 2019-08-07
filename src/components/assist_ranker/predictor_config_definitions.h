// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_
#define COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/assist_ranker/predictor_config.h"

namespace assist_ranker {

#if defined(OS_ANDROID)
extern const base::Feature kContextualSearchRankerQuery;
const PredictorConfig GetContextualSearchPredictorConfig();
#endif  // OS_ANDROID

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_PREDICTOR_CONFIG_DEFINITIONS_H_
