// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/feature.h"

#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {

TEST(UnifiedConsentFeatureTest, FeatureState) {
  // Unified consent is disabled by default.
  EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
  EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());

  {
    ScopedUnifiedConsent scoped_disabled(UnifiedConsentFeatureState::kDisabled);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }

  {
    ScopedUnifiedConsent scoped_no_bump(
        UnifiedConsentFeatureState::kEnabledNoBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }

  {
    ScopedUnifiedConsent scoped_bump(
        UnifiedConsentFeatureState::kEnabledWithBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
    EXPECT_TRUE(IsUnifiedConsentFeatureWithBumpEnabled());
  }
}

TEST(UnifiedConsentFeatureTest, SyncUserConsentSeparateTypeDisabled) {
  // Enable kSyncUserConsentSeparateType
  base::test::ScopedFeatureList scoped_sync_user_consent_separate_type_feature;
  scoped_sync_user_consent_separate_type_feature.InitAndDisableFeature(
      switches::kSyncUserConsentSeparateType);

  {
    base::test::ScopedFeatureList unified_consent_feature_list_;
    unified_consent_feature_list_.InitAndEnableFeature(kUnifiedConsent);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }

  {
    std::map<std::string, std::string> feature_params;
    feature_params[kUnifiedConsentShowBumpParameter] = "true";
    base::test::ScopedFeatureList unified_consent_feature_list_;
    unified_consent_feature_list_.InitAndEnableFeatureWithParameters(
        kUnifiedConsent, feature_params);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }
}

}  // namespace unified_consent
