// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Mirror the constants used in the component installer. Do not share the
// constants, as want to catch inadvertent changes in the tests. The keys will
// will be generated server-side, so any changes need to be intentional and
// coordinated.
static const char kManifestOriginTrialsKey[] = "origin-trials";
static const char kManifestPublicKeyPath[] = "origin-trials.public-key";
static const char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";
static const char kManifestDisabledTokensPath[] =
    "origin-trials.disabled-tokens";
static const char kManifestDisabledTokenSignaturesPath[] =
    "origin-trials.disabled-tokens.signatures";

static const char kExistingPublicKey[] = "existing public key";
static const char kNewPublicKey[] = "new public key";
static const char kExistingDisabledFeature[] = "already disabled";
static const std::vector<std::string> kExistingDisabledFeatures = {
    kExistingDisabledFeature};
static const char kNewDisabledFeature1[] = "newly disabled 1";
static const char kNewDisabledFeature2[] = "newly disabled 2";
static const std::vector<std::string> kNewDisabledFeatures = {
    kNewDisabledFeature1, kNewDisabledFeature2};
static const char kExistingDisabledToken[] = "already disabled token";
static const std::vector<std::string> kExistingDisabledTokens = {
    kExistingDisabledToken};
static const char kNewDisabledToken1[] = "newly disabled token 1";
static const char kNewDisabledToken2[] = "newly disabled token 2";
static const std::vector<std::string> kNewDisabledTokens = {kNewDisabledToken1,
                                                            kNewDisabledToken2};

}  // namespace

namespace component_updater {

class OriginTrialsComponentInstallerTest : public PlatformTest {
 public:
  OriginTrialsComponentInstallerTest() = default;

  OriginTrialsComponentInstallerTest(
      const OriginTrialsComponentInstallerTest&) = delete;
  OriginTrialsComponentInstallerTest& operator=(
      const OriginTrialsComponentInstallerTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedder_support::OriginTrialPrefs::RegisterPrefs(local_state_.registry());
    policy_ = std::make_unique<OriginTrialsComponentInstallerPolicy>();
  }

  void LoadUpdates(base::Value manifest) {
    if (manifest.DictEmpty()) {
      manifest.SetKey(kManifestOriginTrialsKey, base::Value());
    }
    ASSERT_TRUE(policy_->VerifyInstallation(manifest, temp_dir_.GetPath()));
    embedder_support::ReadOriginTrialsConfigAndPopulateLocalState(
        local_state(), std::move(manifest));
  }

  void AddDisabledFeaturesToPrefs(const std::vector<std::string>& features) {
    base::Value disabled_feature_list(base::Value::Type::LIST);
    for (const std::string& feature : features) {
      disabled_feature_list.Append(feature);
    }
    ListPrefUpdate update(
        local_state(), embedder_support::prefs::kOriginTrialDisabledFeatures);
    *update = std::move(disabled_feature_list);
  }

  void CheckDisabledFeaturesPrefs(const std::vector<std::string>& features) {
    ASSERT_FALSE(features.empty());

    ASSERT_TRUE(local_state()->HasPrefPath(
        embedder_support::prefs::kOriginTrialDisabledFeatures));

    const base::Value* disabled_feature_list = local_state()->GetList(
        embedder_support::prefs::kOriginTrialDisabledFeatures);
    ASSERT_TRUE(disabled_feature_list);

    ASSERT_EQ(features.size(), disabled_feature_list->GetList().size());

    for (size_t i = 0; i < features.size(); ++i) {
      const std::string* disabled_feature =
          disabled_feature_list->GetList()[i].GetIfString();
      if (!disabled_feature) {
        ADD_FAILURE() << "Entry not found or not a string at index " << i;
        continue;
      }
      EXPECT_EQ(features[i], *disabled_feature)
          << "Feature lists differ at index " << i;
    }
  }

  void AddDisabledTokensToPrefs(const std::vector<std::string>& tokens) {
    base::Value disabled_token_list(base::Value::Type::LIST);
    for (const std::string& token : tokens) {
      disabled_token_list.Append(token);
    }
    ListPrefUpdate update(local_state(),
                          embedder_support::prefs::kOriginTrialDisabledTokens);
    *update = std::move(disabled_token_list);
  }

  void CheckDisabledTokensPrefs(const std::vector<std::string>& tokens) {
    ASSERT_FALSE(tokens.empty());

    ASSERT_TRUE(local_state()->HasPrefPath(
        embedder_support::prefs::kOriginTrialDisabledTokens));

    const base::Value* disabled_token_list = local_state()->GetList(
        embedder_support::prefs::kOriginTrialDisabledTokens);
    ASSERT_TRUE(disabled_token_list);

    ASSERT_EQ(tokens.size(), disabled_token_list->GetList().size());

    for (size_t i = 0; i < tokens.size(); ++i) {
      const std::string* disabled_token =
          disabled_token_list->GetList()[i].GetIfString();

      if (!disabled_token) {
        ADD_FAILURE() << "Entry not found or not a string at index " << i;
        continue;
      }
      EXPECT_EQ(tokens[i], *disabled_token)
          << "Token lists differ at index " << i;
    }
  }

  PrefService* local_state() { return &local_state_; }

 protected:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
};

TEST_F(OriginTrialsComponentInstallerTest,
       PublicKeyResetToDefaultWhenOverrideMissing) {
  local_state()->SetString(embedder_support::prefs::kOriginTrialPublicKey,
                           kExistingPublicKey);
  ASSERT_EQ(
      kExistingPublicKey,
      local_state()->GetString(embedder_support::prefs::kOriginTrialPublicKey));

  // Load with empty section in manifest
  LoadUpdates(base::Value(base::Value::Type::DICTIONARY));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest, PublicKeySetWhenOverrideExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialPublicKey));

  base::Value manifest(base::Value::Type::DICTIONARY);
  manifest.SetStringPath(kManifestPublicKeyPath, kNewPublicKey);
  LoadUpdates(std::move(manifest));

  EXPECT_EQ(kNewPublicKey, local_state()->GetString(
                               embedder_support::prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListMissing) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  // Load with empty section in manifest
  LoadUpdates(base::Value(base::Value::Type::DICTIONARY));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListEmpty) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_feature_list;
  manifest.SetPath(kManifestDisabledFeaturesPath,
                   std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest, DisabledFeaturesSetWhenListExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_feature_list;
  disabled_feature_list.Append(kNewDisabledFeature1);
  manifest.SetPath(kManifestDisabledFeaturesPath,
                   std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  std::vector<std::string> features = {kNewDisabledFeature1};
  CheckDisabledFeaturesPrefs(features);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesReplacedWhenListExists) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_feature_list;
  for (const std::string& feature : kNewDisabledFeatures) {
    disabled_feature_list.Append(feature);
  }
  manifest.SetPath(kManifestDisabledFeaturesPath,
                   std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  CheckDisabledFeaturesPrefs(kNewDisabledFeatures);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenListMissing) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  // Load with empty section in manifest
  LoadUpdates(base::Value(base::Value::Type::DICTIONARY));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenKeyExistsAndListMissing) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  // Load with disabled tokens key in manifest, but no list values
  base::Value manifest(base::Value::Type::DICTIONARY);
  manifest.SetPath(kManifestDisabledTokensPath, base::Value());

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenListEmpty) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_token_list;
  manifest.SetPath(kManifestDisabledTokenSignaturesPath,
                   std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest, DisabledTokensSetWhenListExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_token_list;
  disabled_token_list.Append(kNewDisabledToken1);
  manifest.SetPath(kManifestDisabledTokenSignaturesPath,
                   std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  std::vector<std::string> tokens = {kNewDisabledToken1};
  CheckDisabledTokensPrefs(tokens);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensReplacedWhenListExists) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value manifest(base::Value::Type::DICTIONARY);
  base::ListValue disabled_token_list;
  for (const std::string& token : kNewDisabledTokens) {
    disabled_token_list.Append(token);
  }
  manifest.SetPath(kManifestDisabledTokenSignaturesPath,
                   std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  CheckDisabledTokensPrefs(kNewDisabledTokens);
}

}  // namespace component_updater
