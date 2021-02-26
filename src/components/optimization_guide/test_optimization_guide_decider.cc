// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/test_optimization_guide_decider.h"

#include "content/public/browser/navigation_handle.h"

namespace optimization_guide {

TestOptimizationGuideDecider::TestOptimizationGuideDecider() = default;
TestOptimizationGuideDecider::~TestOptimizationGuideDecider() = default;

void TestOptimizationGuideDecider::RegisterOptimizationTargets(
    const std::vector<proto::OptimizationTarget>& optimization_targets) {}

void TestOptimizationGuideDecider::ShouldTargetNavigationAsync(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target,
    const base::flat_map<proto::ClientModelFeature, float>&
        client_model_feature_values,
    OptimizationGuideTargetDecisionCallback callback) {
  std::move(callback).Run(OptimizationGuideDecision::kFalse);
}

void TestOptimizationGuideDecider::RegisterOptimizationTypes(
    const std::vector<proto::OptimizationType>& optimization_types) {}

void TestOptimizationGuideDecider::CanApplyOptimizationAsync(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationType optimization_type,
    OptimizationGuideDecisionCallback callback) {
  std::move(callback).Run(OptimizationGuideDecision::kFalse,
                          /*optimization_metadata=*/{});
}

OptimizationGuideDecision TestOptimizationGuideDecider::CanApplyOptimization(
    const GURL& url,
    proto::OptimizationType optimization_type,
    OptimizationMetadata* optimization_metadata) {
  return OptimizationGuideDecision::kFalse;
}

}  // namespace optimization_guide
