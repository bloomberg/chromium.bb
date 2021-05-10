// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/engine/configure_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#endif

namespace syncer {

namespace {

ModelTypeSet GetUserTypes() {
  ModelTypeSet user_types = UserTypes();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // These types only exist when SplitSettingsSync is enabled.
  if (!chromeos::features::IsSplitSettingsSyncEnabled()) {
    user_types.RemoveAll(
        {OS_PREFERENCES, OS_PRIORITY_PREFERENCES, WIFI_CONFIGURATIONS});
  }
#else
  // Ignore all Chrome OS types on non-Chrome OS platforms.
  user_types.RemoveAll({APP_LIST, ARC_PACKAGE, OS_PREFERENCES,
                        OS_PRIORITY_PREFERENCES, PRINTERS,
                        WIFI_CONFIGURATIONS});
#endif
  return user_types;
}

ModelTypeSet GetPreferredUserTypes(
    const SyncUserSettingsImpl& sync_user_settings) {
  return Intersection(UserTypes(), sync_user_settings.GetPreferredDataTypes());
}

class SyncUserSettingsTest : public testing::Test {
 protected:
  SyncUserSettingsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);

    sync_service_crypto_ = std::make_unique<SyncServiceCrypto>(
        /*notify_observers=*/base::DoNothing(),
        /*notify_required_user_action_changed=*/base::DoNothing(),
        /*reconfigure=*/base::DoNothing(),
        /*trusted_vault_client=*/nullptr);
  }

  std::unique_ptr<SyncUserSettingsImpl> MakeSyncUserSettings(
      ModelTypeSet registered_types) {
    return std::make_unique<SyncUserSettingsImpl>(
        sync_service_crypto_.get(), sync_prefs_.get(),
        /*preference_provider=*/nullptr, registered_types);
  }

  // The order of fields matters because it determines destruction order and
  // fields are dependent.
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  std::unique_ptr<SyncServiceCrypto> sync_service_crypto_;
};

TEST_F(SyncUserSettingsTest, PreferredTypesSyncEverything) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  ModelTypeSet expected_types = GetUserTypes();
  EXPECT_TRUE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  for (UserSelectableType type : all_registered_types) {
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/true,
                                         /*selected_type=*/{type});
    EXPECT_EQ(expected_types, GetPreferredUserTypes(*sync_user_settings));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsTest, PreferredTypesSyncAllOsTypes) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_TRUE(sync_user_settings->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(GetUserTypes(), GetPreferredUserTypes(*sync_user_settings));
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/true,
                                           /*selected_types=*/{type});
    EXPECT_EQ(GetUserTypes(), GetPreferredUserTypes(*sync_user_settings));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncUserSettingsTest, PreferredTypesNotKeepEverythingSynced) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*selected_types=*/UserSelectableTypeSet());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    // GetPreferredUserTypes() returns ModelTypes, which includes both browser
    // and OS types. However, this test exercises browser UserSelectableTypes,
    // so disable OS selectable types.
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                           UserSelectableOsTypeSet());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // No user selectable types are enabled, so only the "always preferred" types
  // are preferred.
  ASSERT_EQ(AlwaysPreferredUserTypes(),
            GetPreferredUserTypes(*sync_user_settings));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  for (UserSelectableType type : all_registered_types) {
    ModelTypeSet expected_preferred_types =
        UserSelectableTypeToAllModelTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                         /*selected_types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsTest, PreferredTypesNotAllOsTypesSynced) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  sync_user_settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*selected_types=*/UserSelectableOsTypeSet());
  EXPECT_FALSE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_FALSE(sync_user_settings->IsSyncAllOsTypesEnabled());
  EXPECT_EQ(AlwaysPreferredUserTypes(),
            GetPreferredUserTypes(*sync_user_settings));

  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    ModelTypeSet expected_preferred_types =
        UserSelectableOsTypeToAllModelTypes(type);
    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedOsTypes(/*sync_all_os_types=*/false,
                                           /*selected_types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Device info should always be enabled.
TEST_F(SyncUserSettingsTest, DeviceInfo) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*selected_types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));

  sync_user_settings = MakeSyncUserSettings(ModelTypeSet(DEVICE_INFO));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
}

// User Consents should always be enabled.
TEST_F(SyncUserSettingsTest, UserConsents) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  UserSelectableTypeSet all_registered_types =
      sync_user_settings->GetRegisteredSelectableTypes();
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*selected_types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/all_registered_types);
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));

  sync_user_settings = MakeSyncUserSettings(ModelTypeSet(USER_CONSENTS));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SyncUserSettingsTest, AlwaysPreferredTypes_ChromeOS) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  // Disable all browser types.
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());

  // Disable all OS types.
  sync_user_settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*selected_types=*/UserSelectableOsTypeSet());

  // Important types are still preferred.
  ModelTypeSet preferred_types = sync_user_settings->GetPreferredDataTypes();
  EXPECT_TRUE(preferred_types.Has(DEVICE_INFO));
  EXPECT_TRUE(preferred_types.Has(USER_CONSENTS));
}

TEST_F(SyncUserSettingsTest, AppsAreHandledByOsSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);

  std::unique_ptr<SyncUserSettingsImpl> settings =
      MakeSyncUserSettings(GetUserTypes());

  ASSERT_TRUE(settings->IsSyncEverythingEnabled());
  ASSERT_TRUE(settings->IsSyncAllOsTypesEnabled());

  // App model types are enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Disable browser types.
  settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());

  // App model types are still enabled.
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_TRUE(settings->GetPreferredDataTypes().Has(WEB_APPS));

  // Disable OS types.
  settings->SetSelectedOsTypes(
      /*sync_all_os_types=*/false,
      /*selected_types=*/UserSelectableOsTypeSet());

  // Apps are disabled.
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APP_LIST));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APP_SETTINGS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(APPS));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(ARC_PACKAGE));
  EXPECT_FALSE(settings->GetPreferredDataTypes().Has(WEB_APPS));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(SyncUserSettingsTest, ShouldMutePassphrasePrompt) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  sync_user_settings->MarkPassphrasePromptMutedForCurrentProductVersion();
  EXPECT_TRUE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  // Clearing the preference should unmute the prompt.
  sync_prefs_->ClearPassphrasePromptMutedProductVersion();
  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());
}

TEST_F(SyncUserSettingsTest, ShouldClearPassphrasePromptMuteUponUpgrade) {
  // Mimic an old product version being written to prefs.
  sync_prefs_->SetPassphrasePromptMutedProductVersion(/*major_version=*/73);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(GetUserTypes());

  EXPECT_FALSE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());

  // Muting should still work.
  sync_user_settings->MarkPassphrasePromptMutedForCurrentProductVersion();
  EXPECT_TRUE(
      sync_user_settings->IsPassphrasePromptMutedForCurrentProductVersion());
}

}  // namespace

}  // namespace syncer
