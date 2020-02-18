// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_app_list_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

using apps_helper::DisableApp;
using apps_helper::EnableApp;
using apps_helper::HasSameApps;
using apps_helper::IncognitoDisableApp;
using apps_helper::IncognitoEnableApp;
using apps_helper::InstallApp;
using apps_helper::InstallAppsPendingForSync;
using apps_helper::IsAppEnabled;
using apps_helper::IsIncognitoEnabled;
using apps_helper::UninstallApp;

namespace {

const size_t kNumDefaultApps = 2;

bool AllProfilesHaveSameAppList() {
  return SyncAppListHelper::GetInstance()->AllProfilesHaveSameAppList();
}

const app_list::AppListSyncableService::SyncItem* GetSyncItem(
    Profile* profile,
    const std::string& app_id) {
  app_list::AppListSyncableService* service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  return service->GetSyncItem(app_id);
}

}  // namespace

class TwoClientAppListSyncTest : public SyncTest {
 public:
  TwoClientAppListSyncTest() : SyncTest(TWO_CLIENT) { DisableVerifier(); }

  ~TwoClientAppListSyncTest() override {}

  // SyncTest
  bool SetupClients() override {
    if (!SyncTest::SetupClients())
      return false;

    // Init SyncAppListHelper to ensure that the extension system is initialized
    // for each Profile.
    SyncAppListHelper::GetInstance();
    return true;
  }

  bool SetupSync() override {
    if (!SyncTest::SetupSync())
      return false;
    WaitForExtensionServicesToLoad();
    return true;
  }

  void AwaitQuiescenceAndInstallAppsPendingForSync() {
    ASSERT_TRUE(AwaitQuiescence());
    InstallAppsPendingForSync(GetProfile(0));
    InstallAppsPendingForSync(GetProfile(1));
  }

 private:
  void WaitForExtensionServicesToLoad() {
    for (int i = 0; i < num_clients(); ++i)
      WaitForExtensionsServiceToLoadForProfile(GetProfile(i));
  }

  void WaitForExtensionsServiceToLoadForProfile(Profile* profile) {
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    if (extension_service && extension_service->is_ready())
      return;
    content::WindowedNotificationObserver extensions_loaded_observer(
        extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
        content::NotificationService::AllSources());
    extensions_loaded_observer.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(TwoClientAppListSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithSameApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, some on only one client, some on only the
// other, and sync.  Both clients should end up with all apps, and the app and
// page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, StartWithDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());

  AwaitQuiescenceAndInstallAppsPendingForSync();

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, then sync.  Then install some apps on only
// one client, some on only the other, and then sync again.  Both clients should
// end up with all apps, and the app and page ordinals should be identical.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, InstallDifferentApps) {
  ASSERT_TRUE(SetupClients());

  int i = 0;

  const int kNumCommonApps = 5;
  for (int j = 0; j < kNumCommonApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(GetProfile(1), i);
  }

  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(AwaitQuiescence());

  const int kNumProfile0Apps = 10;
  for (int j = 0; j < kNumProfile0Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(0), i);
  }

  const int kNumProfile1Apps = 10;
  for (int j = 0; j < kNumProfile1Apps; ++i, ++j) {
    std::string id = InstallApp(GetProfile(1), i);
  }

  AwaitQuiescenceAndInstallAppsPendingForSync();

  // Verify the app lists, but ignore absolute position values, checking only
  // relative positions (see note in app_list_syncable_service.h).
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Install) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Uninstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install an app on one client, then sync. Then uninstall the app on the first
// client and sync again. Now install a new app on the first client and sync.
// Both client should only have the second app, with identical app and page
// ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UninstallThenInstall) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  UninstallApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 1);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Merge) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());

  UninstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(0), 1);

  InstallApp(GetProfile(0), 2);
  InstallApp(GetProfile(1), 2);

  InstallApp(GetProfile(1), 3);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateEnableDisableApp) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsAppEnabled(GetProfile(1), 0));

  DisableApp(GetProfile(0), 0);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_FALSE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsAppEnabled(GetProfile(1), 0));

  EnableApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_TRUE(IsAppEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsAppEnabled(GetProfile(1), 0));
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, UpdateIncognitoEnableDisable) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(1), 0));

  IncognitoEnableApp(GetProfile(0), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_TRUE(IsIncognitoEnabled(GetProfile(1), 0));

  IncognitoDisableApp(GetProfile(1), 0);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(0), 0));
  ASSERT_FALSE(IsIncognitoEnabled(GetProfile(1), 0));
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Disable APP_LIST by disabling kApps since APP_LIST is in kApps groups.
  ASSERT_TRUE(
      GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kApps));
  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllProfilesHaveSameAppList());

  // Enable APP_LIST by enabling kApps since APP_LIST is in kApps groups.
  ASSERT_TRUE(
      GetClient(1)->EnableSyncForType(syncer::UserSelectableType::kApps));
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Disable sync for the second client and then install an app on the first
// client, then enable sync on the second client. Both clients should have the
// same app with identical app and page ordinals.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  InstallApp(GetProfile(0), 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllProfilesHaveSameAppList());

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install some apps on both clients, then sync. Move an app on one client
// and sync. Both clients should have the updated position for the app.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, Move) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i)
    InstallApp(GetProfile(1), i);

  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

// Install a Default App on both clients, then sync. Remove the app on one
// client and sync. Ensure that the app is removed on the other client and
// that a REMOVE_DEFAULT_APP entry exists.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, RemoveDefault) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());

  // Install a non-default app.
  InstallApp(GetProfile(0), 0);
  InstallApp(GetProfile(1), 0);

  // Install a default app in Profile 0 only.
  const int default_app_index = 1;
  std::string default_app_id = InstallApp(GetProfile(0), default_app_index);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Flag Default app in Profile 1.
  using ALSS = app_list::AppListSyncableService;
  EXPECT_FALSE(ALSS::AppIsDefaultForTest(GetProfile(1), default_app_id));
  ALSS::SetAppIsDefaultForTest(GetProfile(1), default_app_id);
  EXPECT_TRUE(ALSS::AppIsDefaultForTest(GetProfile(1), default_app_id));

  // Remove the default app in Profile 0 and verifier, ensure it was removed
  // in Profile 1.
  UninstallApp(GetProfile(0), default_app_index);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Ensure that a REMOVE_DEFAULT_APP SyncItem entry exists in Profile 1.
  const ALSS::SyncItem* sync_item = GetSyncItem(GetProfile(1), default_app_id);
  ASSERT_TRUE(sync_item);
  ASSERT_EQ(sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP,
            sync_item->item_type);

  // Re-Install the same app in Profile 0.
  std::string app_id2 = InstallApp(GetProfile(0), default_app_index);
  EXPECT_EQ(default_app_id, app_id2);
  sync_item = GetSyncItem(GetProfile(0), app_id2);
  EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);
  AwaitQuiescenceAndInstallAppsPendingForSync();

  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Ensure that the REMOVE_DEFAULT_APP SyncItem entry in Profile 1 is replaced
  // with an APP entry after an install.
  sync_item = GetSyncItem(GetProfile(1), app_id2);
  ASSERT_TRUE(sync_item);
  EXPECT_EQ(sync_pb::AppListSpecifics::TYPE_APP, sync_item->item_type);
}

#if !defined(OS_MACOSX)

// Install some apps on both clients, then sync. Move an app on one client
// to a folder and sync. The app lists, including folders, should match.
IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, MoveToFolder) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 5;
  std::vector<std::string> app_ids;
  for (int i = 0; i < kNumApps; ++i) {
    app_ids.push_back(InstallApp(GetProfile(0), i));
    InstallApp(GetProfile(1), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  size_t index = 2u;
  std::string folder_id = "Folder 0";
  SyncAppListHelper::GetInstance()->MoveAppToFolder(GetProfile(0),
                                                    app_ids[index], folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

IN_PROC_BROWSER_TEST_F(TwoClientAppListSyncTest, FolderAddRemove) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  const int kNumApps = 10;
  std::vector<std::string> app_ids;
  for (int i = 0; i < kNumApps; ++i) {
    app_ids.push_back(InstallApp(GetProfile(0), i));
    InstallApp(GetProfile(1), i);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Move a few apps to a folder.
  const size_t kNumAppsToMove = 3;
  std::string folder_id = "Folder 0";
  // The folder will be created at the end of the list; always move the
  // non default items in the list.
  // Note: We don't care about the order of items in Chrome, so when we
  //       changes a file's folder, its index in the list remains unchanged.
  //       The |kNumAppsToMove| items to move are
  //       app_ids[item_index..(item_index+kNumAppsToMove-1)].
  size_t item_index = kNumDefaultApps;
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), app_ids[item_index + i], folder_id);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Remove one app from the folder.
  SyncAppListHelper::GetInstance()->MoveAppFromFolder(GetProfile(0), app_ids[0],
                                                      folder_id);

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Remove remaining apps from the folder (deletes folder).
  for (size_t i = 1; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppFromFolder(GetProfile(0),
                                                        app_ids[0], folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());

  // Move apps back to a (new) folder.
  for (size_t i = 0; i < kNumAppsToMove; ++i) {
    SyncAppListHelper::GetInstance()->MoveAppToFolder(
        GetProfile(0), app_ids[item_index], folder_id);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllProfilesHaveSameAppList());
}

#endif  // !defined(OS_MACOSX)
