// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/contextual/contextual_suggestions_features.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_suggestions {

TEST(ContextualSuggestionsFeaturesTest, GetMinimumConfidence_DefaultValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kContextualSuggestionsButton);
  EXPECT_EQ(0.75, GetMinimumConfidence());
}

TEST(ContextualSuggestionsFeaturesTest, GetMinimumConfidence_ParamValue) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["minimum_confidence"] = "0.6";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ(0.6, GetMinimumConfidence());
}

TEST(ContextualSuggestionsFeaturesTest, GetMinimumConfidence_Minimum) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["minimum_confidence"] = "-0.6";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ(0.0, GetMinimumConfidence());
}

TEST(ContextualSuggestionsFeaturesTest, GetMinimumConfidence_Maximum) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters["minimum_confidence"] = "1.6";
  feature_list.InitAndEnableFeatureWithParameters(kContextualSuggestionsButton,
                                                  parameters);
  EXPECT_EQ(1.0, GetMinimumConfidence());
}

}  // namespace contextual_suggestions
