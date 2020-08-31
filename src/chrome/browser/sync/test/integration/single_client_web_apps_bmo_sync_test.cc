// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using syncer::UserSelectableType;
using syncer::UserSelectableTypeSet;

namespace {

#if defined(OS_CHROMEOS)

// These tests test only the new Web Apps system with next generation sync.
// (BMO flag is always enabled). WEB_APPS and WebAppSyncBridge are always on.
//
// Chrome OS syncs apps as an OS type.
class SingleClientWebAppsBmoOsSyncTest : public OsSyncTest {
 public:
  SingleClientWebAppsBmoOsSyncTest() : OsSyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(features::kDesktopPWAsWithoutExtensions);
  }
  ~SingleClientWebAppsBmoOsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsBmoOsSyncTest,
                       DisablingOsSyncFeatureDisablesDataType) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

  settings->SetOsSyncFeatureEnabled(false);
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
}

#else   // !defined(OS_CHROMEOS)

// These tests test only the new Web Apps system with next generation sync.
// (BMO flag is always enabled). WEB_APPS and WebAppSyncBridge are always on.
//
// See also TwoClientWebAppsBmoSyncTest.
class SingleClientWebAppsBmoSyncTest : public SyncTest {
 public:
  SingleClientWebAppsBmoSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitAndEnableFeature(features::kDesktopPWAsWithoutExtensions);
  }
  ~SingleClientWebAppsBmoSyncTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWebAppsBmoSyncTest,
                       DisablingSelectedTypeDisablesModelType) {
  ASSERT_TRUE(SetupSync());
  syncer::ProfileSyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();
  ASSERT_TRUE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));

  settings->SetSelectedTypes(false, UserSelectableTypeSet());
  ASSERT_FALSE(settings->GetSelectedTypes().Has(UserSelectableType::kApps));
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::WEB_APPS));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
