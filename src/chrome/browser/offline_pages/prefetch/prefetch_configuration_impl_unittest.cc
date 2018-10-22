// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_configuration_impl.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchConfigurationImplTest : public testing::Test {
 public:
  PrefetchConfigurationImplTest()
      : pref_service_(), prefetch_config_(&pref_service_) {}
  ~PrefetchConfigurationImplTest() override = default;

  void SetUp() override {
    PrefetchConfigurationImpl::RegisterPrefs(pref_service_.registry());
  }

  PrefService& prefs() { return pref_service_; }
  PrefetchConfigurationImpl& config() { return prefetch_config_; }

 protected:
  TestingPrefServiceSimple pref_service_;
  PrefetchConfigurationImpl prefetch_config_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchConfigurationImplTest);
};

TEST_F(PrefetchConfigurationImplTest, EnabledInSettingsByDefault) {
  EXPECT_TRUE(prefs().GetBoolean(prefs::kOfflinePrefetchEnabled));
  EXPECT_TRUE(config().IsPrefetchingEnabledBySettings());
}

TEST_F(PrefetchConfigurationImplTest, WhenPrefetchFlagIsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPrefetchingOfflinePagesFeature);

  // Disable in settings and check.
  config().SetPrefetchingEnabledInSettings(false);
  EXPECT_FALSE(config().IsPrefetchingEnabledBySettings());
  EXPECT_FALSE(config().IsPrefetchingEnabled());

  // Re-enable in settings and check.
  config().SetPrefetchingEnabledInSettings(true);
  EXPECT_TRUE(config().IsPrefetchingEnabledBySettings());
  EXPECT_TRUE(config().IsPrefetchingEnabled());
}

TEST_F(PrefetchConfigurationImplTest, WhenPrefetchFlagIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);

  // Disable in settings and check.
  config().SetPrefetchingEnabledInSettings(false);
  EXPECT_FALSE(config().IsPrefetchingEnabledBySettings());
  EXPECT_FALSE(config().IsPrefetchingEnabled());

  // Re-enable in settings and check.
  config().SetPrefetchingEnabledInSettings(true);
  EXPECT_TRUE(config().IsPrefetchingEnabledBySettings());
  EXPECT_FALSE(config().IsPrefetchingEnabled());
}

}  // namespace offline_pages
