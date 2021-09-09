// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include <memory>
#include "base/test/task_environment.h"

#include "base/test/scoped_feature_list.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfFeaturesTest : public testing::Test {
 public:
  SendTabToSelfFeaturesTest() {
    syncer::SyncPrefs::RegisterProfilePrefs(prefs_.registry());
    sync_prefs_ = std::make_unique<syncer::SyncPrefs>(&prefs_);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(SendTabToSelfFeaturesTest, ReceivingEnabledIfSyncTheFeatureEnabled) {
  sync_prefs_->SetSyncRequested(true);
  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{syncer::UserSelectableType::kTabs});

  EXPECT_TRUE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(SendTabToSelfFeaturesTest,
       ReceivingDisabledIfSyncTheFeatureEnabledButStopped) {
  sync_prefs_->SetSyncRequested(false);
  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{syncer::UserSelectableType::kTabs});

  EXPECT_FALSE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(SendTabToSelfFeaturesTest,
       ReceivingDisabledIfSyncTheFeatureEnabledButTabsNotSelected) {
  sync_prefs_->SetSyncRequested(true);
  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{});

  EXPECT_FALSE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(SendTabToSelfFeaturesTest, ReceivingDisabledIfSyncTheFeatureDisabled) {
  features_.InitAndDisableFeature(kSendTabToSelfWhenSignedIn);
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{});

  EXPECT_FALSE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(
    SendTabToSelfFeaturesTest,
    ReceivingDisabledIfSyncSetupIncomplete__SendTabToSelfWhenSignedInEnabled) {
  features_.InitAndDisableFeature(kSendTabToSelfWhenSignedIn);

  sync_prefs_->SetSyncRequested(true);
  // Skip setting FirstSetupComplete.
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{syncer::UserSelectableType::kTabs});

  // Should wait for the setup to complete.
  EXPECT_FALSE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(SendTabToSelfFeaturesTest,
       ReceivingEnabledIfSyncSetupIncomplete_SendTabToSelfWhenSignedInEnabled) {
  features_.InitAndEnableFeature(kSendTabToSelfWhenSignedIn);

  sync_prefs_->SetSyncRequested(true);
  // Skip setting FirstSetupComplete.
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{syncer::UserSelectableType::kTabs});

  // While the setup isn't complete, the client is still treated as having
  // sync-the-feature disabled.
  EXPECT_TRUE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

TEST_F(
    SendTabToSelfFeaturesTest,
    ReceivingEnabledIfSyncTheFeatureDisabled_SendTabToSelfWhenSignedInEnabled) {
  features_.InitAndEnableFeature(kSendTabToSelfWhenSignedIn);
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/syncer::UserSelectableTypeSet::All(),
      /*selected_types=*/{});

  EXPECT_TRUE(IsReceivingEnabledByUserOnThisDevice(&prefs_));
}

}  // namespace

}  // namespace send_tab_to_self
