// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_preference_helper.h"

#include "components/contextual_search/core/browser/contextual_search_preference.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests the Contextual Search Preference helper's native functionality.
class ContextualSearchPreferenceHelperTest : public testing::Test {
 public:
  ContextualSearchPreferenceHelperTest() {}

  void SetUp() override {
    preference_ = contextual_search::ContextualSearchPreference::GetInstance();
    helper_.reset(new ContextualSearchPreferenceHelper(nullptr, nullptr));
    // Default to a user that has disabled the feature.
    pref_service_.registry()->RegisterStringPref(
        contextual_search::GetPrefName(), "false");
  }

 protected:
  // The helper under test.
  std::unique_ptr<ContextualSearchPreferenceHelper> helper_;
  // The preference that captures the previous-state metadata.
  contextual_search::ContextualSearchPreference* preference_;
  // a PrefService for testing.
  TestingPrefServiceSimple pref_service_;
};

// Tests that we start out not knowing the previous state.
TEST_F(ContextualSearchPreferenceHelperTest, GetMetadataWithoutGivingConsent) {
  EXPECT_EQ(UNKNOWN, helper_->GetPreferenceMetadata(nullptr, nullptr));
}

TEST_F(ContextualSearchPreferenceHelperTest, DisabledUsers) {
  preference_->EnableIfUndecided(&pref_service_);
  EXPECT_EQ(WAS_DECIDED, helper_->GetPreferenceMetadata(nullptr, nullptr));
}

TEST_F(ContextualSearchPreferenceHelperTest, UndecidedUsers) {
  pref_service_.SetString("search.contextual_search_enabled", "");
  preference_->EnableIfUndecided(&pref_service_);
  EXPECT_EQ(WAS_UNDECIDED, helper_->GetPreferenceMetadata(nullptr, nullptr));
}

TEST_F(ContextualSearchPreferenceHelperTest, DecidedUsers) {
  pref_service_.SetString("search.contextual_search_enabled", "true");
  preference_->EnableIfUndecided(&pref_service_);
  EXPECT_EQ(WAS_DECIDED, helper_->GetPreferenceMetadata(nullptr, nullptr));
}
