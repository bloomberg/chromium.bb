// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/feature_policy/document_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom.h"

namespace blink {
namespace {

using DocumentPolicyTest = ::testing::Test;

// Helper function to convert literal to FeatureState.
template <class T>
DocumentPolicy::FeatureState FeatureState(
    std::vector<std::pair<int32_t, T>> literal) {
  DocumentPolicy::FeatureState result;
  for (const auto& entry : literal) {
    result.insert({static_cast<mojom::DocumentPolicyFeature>(entry.first),
                   PolicyValue(entry.second)});
  }
  return result;
}

TEST_F(DocumentPolicyTest, MergeFeatureState) {
  EXPECT_EQ(DocumentPolicy::MergeFeatureState(
                FeatureState<bool>(
                    {{1, false}, {2, false}, {3, true}, {4, true}, {5, false}}),
                FeatureState<bool>(
                    {{2, true}, {3, true}, {4, false}, {5, false}, {6, true}})),
            FeatureState<bool>({{1, false},
                                {2, false},
                                {3, true},
                                {4, false},
                                {5, false},
                                {6, true}}));
  EXPECT_EQ(
      DocumentPolicy::MergeFeatureState(
          FeatureState<double>({{1, 1.0}, {2, 1.0}, {3, 1.0}, {4, 0.5}}),
          FeatureState<double>({{2, 0.5}, {3, 1.0}, {4, 1.0}, {5, 1.0}})),
      FeatureState<double>({{1, 1.0}, {2, 0.5}, {3, 1.0}, {4, 0.5}, {5, 1.0}}));
}

// IsPolicyCompatible should use default value for incoming policy when required
// policy specifies a value for a feature and incoming policy is missing value
// for that feature.
TEST_F(DocumentPolicyTest, IsPolicyCompatible) {
  mojom::DocumentPolicyFeature feature =
      mojom::DocumentPolicyFeature::kUnoptimizedLosslessImages;
  double default_policy_value =
      GetDocumentPolicyFeatureInfoMap().at(feature).default_value.DoubleValue();
  // Cap the default_policy_value, as it can be INF.
  double strict_policy_value =
      default_policy_value > 1.0 ? 1.0 : default_policy_value / 2;

  EXPECT_FALSE(DocumentPolicy::IsPolicyCompatible(
      DocumentPolicy::FeatureState{
          {feature, PolicyValue(strict_policy_value)}}, /* required policy */
      DocumentPolicy::FeatureState{}                    /* incoming policy */
      ));
}

}  // namespace
}  // namespace blink
