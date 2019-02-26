// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_in_omnibox.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL kValidSearchResultsPage =
    GURL("https://www.google.com/search?q=foo+query");

}  // namespace

class QueryInOmniboxTest : public testing::Test {
 protected:
  QueryInOmniboxTest()
      : omnibox_client_(new TestOmniboxClient),
        model_(new QueryInOmnibox(omnibox_client_->GetAutocompleteClassifier(),
                                  omnibox_client_->GetTemplateURLService())) {}

  QueryInOmnibox* model() { return model_.get(); }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<QueryInOmnibox> model_;
};

TEST_F(QueryInOmniboxTest, FeatureFlagWorks) {
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     false, kValidSearchResultsPage, nullptr));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     false, kValidSearchResultsPage, nullptr));
}

TEST_F(QueryInOmniboxTest, SecurityLevel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     false, kValidSearchResultsPage, nullptr));
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::EV_SECURE,
                                     false, kValidSearchResultsPage, nullptr));

  // Insecure levels should not be allowed to display search terms.
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::NONE, false,
                                     kValidSearchResultsPage, nullptr));
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::DANGEROUS,
                                     false, kValidSearchResultsPage, nullptr));

  // But respect the flag on to ignore the security level.
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::NONE, true,
                                     kValidSearchResultsPage, nullptr));
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::DANGEROUS,
                                     true, kValidSearchResultsPage, nullptr));
}

TEST_F(QueryInOmniboxTest, DefaultSearchProviderWithAndWithoutQuery) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  base::string16 result;
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     false, kValidSearchResultsPage, &result));
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), result);

  const GURL kDefaultSearchProviderURLWithNoQuery(
      "https://www.google.com/maps");
  result.clear();
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, false,
      kDefaultSearchProviderURLWithNoQuery, &result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(QueryInOmniboxTest, NonDefaultSearchProvider) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kNonDefaultSearchProvider(
      "https://search.yahoo.com/search?ei=UTF-8&fr=crmas&p=foo+query");
  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, false, kNonDefaultSearchProvider,
      &result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(QueryInOmniboxTest, LookalikeURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kLookalikeURLQuery(
      "https://www.google.com/search?q=lookalike.com");
  base::string16 result;
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     false, kLookalikeURLQuery, &result));
  EXPECT_EQ(base::string16(), result);
}
