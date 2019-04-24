// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints_util.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "components/previews/core/previews_features.h"
#include "url/gurl.h"

namespace previews {

bool IsDisabledExperimentalOptimization(
    const optimization_guide::proto::Optimization& optimization) {
  // If this optimization has been marked with an experiment name, consider it
  // disabled unless an experiment with that name is running. Experiment names
  // are configured with the experiment_name parameter to the
  // kOptimizationHintsExperiments feature.
  //
  // If kOptimizationHintsExperiments is disabled, getting the param value
  // returns an empty string. Since experiment names are not allowed to be
  // empty strings, all experiments will be disabled if the feature is
  // disabled.
  if (optimization.has_experiment_name() &&
      !optimization.experiment_name().empty() &&
      optimization.experiment_name() !=
          base::GetFieldTrialParamValueByFeature(
              features::kOptimizationHintsExperiments,
              features::kOptimizationHintsExperimentNameParam)) {
    return true;
  }
  return false;
}

const optimization_guide::proto::PageHint* FindPageHintForURL(
    const GURL& gurl,
    const optimization_guide::proto::Hint* hint) {
  if (!hint) {
    return nullptr;
  }

  for (const auto& page_hint : hint->page_hints()) {
    if (page_hint.page_pattern().empty()) {
      continue;
    }
    optimization_guide::URLPatternWithWildcards url_pattern(
        page_hint.page_pattern());
    if (url_pattern.Matches(gurl.spec())) {
      // Return the first matching page hint.
      return &page_hint;
    }
  }
  return nullptr;
}

}  // namespace previews
