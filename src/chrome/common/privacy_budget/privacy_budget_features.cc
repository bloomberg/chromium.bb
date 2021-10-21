// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/privacy_budget/privacy_budget_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"

namespace features {

const base::Feature kIdentifiabilityStudy = {"IdentifiabilityStudy",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kIdentifiabilityStudyGeneration = {
    &kIdentifiabilityStudy, "Gen", 0};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedMetrics = {
    &kIdentifiabilityStudy, "BlockedHashes", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockedTypes = {
    &kIdentifiabilityStudy, "BlockedTypes", ""};

const base::FeatureParam<int> kIdentifiabilityStudyExpectedSurfaceCount = {
    &kIdentifiabilityStudy, "Rho", 0};

const base::FeatureParam<int> kIdentifiabilityStudyActiveSurfaceBudget = {
    &kIdentifiabilityStudy, "Max", kMaxIdentifiabilityStudyActiveSurfaceBudget};

const base::FeatureParam<std::string> kIdentifiabilityStudyPerHashCost = {
    &kIdentifiabilityStudy, "HashCost", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyPerTypeCost = {
    &kIdentifiabilityStudy, "TypeCost", ""};

const base::FeatureParam<std::string>
    kIdentifiabilityStudySurfaceEquivalenceClasses = {&kIdentifiabilityStudy,
                                                      "Classes", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlocks = {
    &kIdentifiabilityStudy, "Blocks", ""};

const base::FeatureParam<std::string> kIdentifiabilityStudyBlockWeights = {
    &kIdentifiabilityStudy, "BlockWeights", ""};

}  // namespace features
