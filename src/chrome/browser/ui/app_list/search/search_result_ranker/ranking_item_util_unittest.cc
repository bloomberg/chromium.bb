// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using test::TestAppListControllerDelegate;
using testing::Eq;

namespace app_list {

class RankingItemUtilTest : public AppListTestBase {
 public:
  RankingItemUtilTest() {}
  ~RankingItemUtilTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();
  }

  void SetAdaptiveRankerParams(
      const std::map<std::string, std::string>& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        app_list_features::kEnableQueryBasedMixedTypesRanker, params);
  }

  std::unique_ptr<OmniboxResult> MakeOmniboxResult(
      AutocompleteMatchType::Type type) {
    AutocompleteMatch match;
    match.type = type;
    return std::make_unique<OmniboxResult>(profile_.get(),
                                           app_list_controller_delegate_.get(),
                                           nullptr, match, false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestAppListControllerDelegate> app_list_controller_delegate_;
};

TEST_F(RankingItemUtilTest, OmniboxSubtypeReturnedWithFinchParameterOn) {
  SetAdaptiveRankerParams({{"expand_omnibox_types", "true"}});
  std::unique_ptr<OmniboxResult> result =
      MakeOmniboxResult(AutocompleteMatchType::HISTORY_URL);
  RankingItemType type = RankingItemTypeFromSearchResult(*result.get());
  EXPECT_EQ(type, RankingItemType::kOmniboxHistory);
}

}  // namespace app_list
