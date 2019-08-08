// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints_util.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "components/previews/core/previews_features.h"
#include "url/gurl.h"

namespace previews {

bool IsDisabledPerOptimizationHintExperiment(
    const optimization_guide::proto::Optimization& optimization) {
  // First check if optimization depends on an experiment being enabled.
  if (optimization.has_experiment_name() &&
      !optimization.experiment_name().empty() &&
      optimization.experiment_name() !=
          base::GetFieldTrialParamValueByFeature(
              features::kOptimizationHintsExperiments,
              features::kOptimizationHintsExperimentNameParam)) {
    return true;
  }
  // Now check if optimization depends on an experiment not being enabled.
  if (optimization.has_excluded_experiment_name() &&
      !optimization.excluded_experiment_name().empty() &&
      optimization.excluded_experiment_name() ==
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

std::string HashHostForDictionary(const std::string& host) {
  return base::StringPrintf("%x", base::PersistentHash(host));
}

}  // namespace previews
