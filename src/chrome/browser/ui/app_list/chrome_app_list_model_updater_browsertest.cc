// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"

namespace {

constexpr char kOemAppId[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";

}  // namespace

class OemAppPositionTest : public ash::LoginManagerTest {
 public:
  OemAppPositionTest() { login_mixin_.AppendRegularUsers(1); }
  OemAppPositionTest(const OemAppPositionTest&) = delete;
  OemAppPositionTest& operator=(const OemAppPositionTest&) = delete;
  ~OemAppPositionTest() override = default;

  // LoginManagerTest:
  bool SetUpUserDataDirectory() override {
    // Create test user profile directory and copy extensions and preferences
    // from the test data directory to it.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    const std::string& email =
        login_mixin_.users()[0].account_id.GetUserEmail();
    const std::string user_id_hash =
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(email);
    const base::FilePath user_profile_path = user_data_dir.Append(
        chromeos::ProfileHelper::GetUserProfileDir(user_id_hash));
    base::CreateDirectory(user_profile_path);

    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
    src_dir = src_dir.AppendASCII("extensions").AppendASCII("app_list_oem");

    base::CopyFile(src_dir.Append(chrome::kPreferencesFilename),
                   user_profile_path.Append(chrome::kPreferencesFilename));
    base::CopyDirectory(src_dir.AppendASCII("Extensions"), user_profile_path,
                        true);
    return true;
  }

  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

class ChromeAppListModelUpdaterTest : public extensions::ExtensionBrowserTest {
 public:
  ChromeAppListModelUpdaterTest() = default;
  ~ChromeAppListModelUpdaterTest() override = default;
  ChromeAppListModelUpdaterTest(const ChromeAppListModelUpdaterTest& other) =
      delete;
  ChromeAppListModelUpdaterTest& operator=(
      const ChromeAppListModelUpdaterTest& other) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();
  }

  ash::AppListTestApi app_list_test_api_;
};

using AppPositionReorderingTest = ChromeAppListModelUpdaterTest;

class ChromeAppListModelUpdaterProductivityLauncherTest
    : public ChromeAppListModelUpdaterTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      ash::features::kProductivityLauncher};
};

// Tests that an Oem app and its folder are created with valid positions after
// sign-in.
IN_PROC_BROWSER_TEST_F(OemAppPositionTest, ValidOemAppPosition) {
  LoginUser(login_mixin_.users()[0].account_id);

  // Ensure apps that are installed upon sign-in are registered with the App
  // Service, resolving any pending messages as a result of running async
  // callbacks.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  proxy->FlushMojoCallsForTesting();

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  client->UpdateProfile();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Ensure async callbacks are run.
  base::RunLoop().RunUntilIdle();

  const ChromeAppListItem* oem_app = model_updater->FindItem(kOemAppId);
  ASSERT_TRUE(oem_app);
  EXPECT_TRUE(oem_app->position().IsValid());

  const ChromeAppListItem* oem_folder =
      model_updater->FindItem(ash::kOemFolderId);
  ASSERT_TRUE(oem_folder);
  EXPECT_TRUE(oem_folder->position().IsValid());
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       GetPositionBeforeFirstItemTest) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);

  // Default apps will be present but we add an app to guarantee there will be
  // at least 1 app.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  syncer::StringOrdinal position_before_first_item =
      model_updater->GetPositionBeforeFirstItem();

  // Check that position is before all items in the list.
  for (const auto& id : top_level_id_list) {
    ChromeAppListItem* item = model_updater->FindItem(id);
    ASSERT_TRUE(position_before_first_item.LessThan(item->position()));
  }

  // Move app to the front.
  app_list_test_api_.MoveItemToPosition(app1_id, 0);

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  syncer::StringOrdinal new_position_before_first_item =
      model_updater->GetPositionBeforeFirstItem();

  // Re-check that position is before all items in the list.
  for (const auto& id : reordered_top_level_id_list) {
    ChromeAppListItem* item = model_updater->FindItem(id);
    ASSERT_TRUE(new_position_before_first_item.LessThan(item->position()));
  }
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       PRE_ReorderAppPositionInTopLevelAppList) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  size_t top_level_id_list_size = top_level_id_list.size();

  // This test ignores the default apps and we don't test the exact
  // |top_level_id_list| size here.
  ASSERT_GE(top_level_id_list_size, 3u);

  ASSERT_EQ(top_level_id_list[top_level_id_list_size - 3], app1_id);
  ASSERT_EQ(top_level_id_list[top_level_id_list_size - 2], app2_id);
  ASSERT_EQ(top_level_id_list[top_level_id_list_size - 1], app3_id);

  // After the move operation, app3 should be at index 0 and app1 should be at
  // index 1. App2 stays at the last position in the item list.
  app_list_test_api_.MoveItemToPosition(app1_id, 0);
  app_list_test_api_.MoveItemToPosition(app3_id, 0);

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();

  EXPECT_EQ(top_level_id_list_size, reordered_top_level_id_list.size());
  EXPECT_EQ(reordered_top_level_id_list[0], app3_id);
  EXPECT_EQ(reordered_top_level_id_list[1], app1_id);
  EXPECT_EQ(reordered_top_level_id_list.back(), app2_id);
}

// Tests if the app position changed in the top level persist after the system
// restarts.
IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       ReorderAppPositionInTopLevelAppList) {
  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  std::vector<std::string> reordered_top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();

  // This test ignores the default apps and we don't test the exact
  // |reordered_top_level_id_list| size here.
  ASSERT_GE(reordered_top_level_id_list.size(), 3u);

  EXPECT_EQ(reordered_top_level_id_list[0], app3_id);
  EXPECT_EQ(reordered_top_level_id_list[1], app1_id);
  EXPECT_EQ(reordered_top_level_id_list.back(), app2_id);
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       PRE_ReorderAppPositionInFolder) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  // Create a folder with app1, app2 and app3 in order.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id, app3_id});

  std::vector<std::string> original_id_list{app1_id, app2_id, app3_id};
  ASSERT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), original_id_list);

  // Change an app position in the folder.
  app_list_test_api_.MoveItemToPosition(app1_id, 2);

  std::vector<std::string> reordered_id_list{app2_id, app3_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}

// Tests if the app position changed in a folder persist after the system
// restarts.
IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest, ReorderAppPositionInFolder) {
  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  std::string folder_id = app_list_test_api_.GetFolderId(app1_id);
  // Check if the three apps are still in the same folder.
  ASSERT_FALSE(folder_id.empty());
  ASSERT_EQ(app_list_test_api_.GetFolderId(app2_id), folder_id);
  ASSERT_EQ(app_list_test_api_.GetFolderId(app3_id), folder_id);

  std::vector<std::string> reordered_id_list{app2_id, app3_id, app1_id};
  EXPECT_EQ(app_list_test_api_.GetAppIdsInFolder(folder_id), reordered_id_list);
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest, PRE_UnmergeTwoItemFolder) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());
  // App3 is the same app as app1 in |test_data_dir_|. Take app4 as the third
  // app in this test.
  const std::string app3_id =
      LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app3_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});

  // Create a folder with app1, app2 and app3 in order.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id, app2_id});

  ash::AppListModel* model = app_list_test_api_.GetAppListModel();
  ash::AppListItem* app2_item = model->FindItem(app2_id);
  ASSERT_TRUE(app2_item);

  ash::AppListItem* app3_item = model->FindItem(app3_id);
  ASSERT_TRUE(app3_item);

  model->MoveItemToRootAt(app2_item, app3_item->position().CreateAfter());

  // Get last 3 items (the grid may have default items, in addition to the ones
  // installed by the test).
  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  ASSERT_GT(top_level_id_list.size(), 2u);
  EXPECT_FALSE(base::Contains(top_level_id_list, folder_id));

  std::vector<std::string> trailing_items = {
      top_level_id_list[top_level_id_list.size() - 3],
      top_level_id_list[top_level_id_list.size() - 2],
      top_level_id_list[top_level_id_list.size() - 1],
  };

  EXPECT_EQ(std::vector<std::string>({app1_id, app3_id, app2_id}),
            trailing_items);
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest, UnmergeTwoItemFolder) {
  const std::string app1_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app1"))
          ->id();
  const std::string app2_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app2"))
          ->id();
  const std::string app3_id =
      GetExtensionByPath(extension_registry()->enabled_extensions(),
                         test_data_dir_.AppendASCII("app4"))
          ->id();

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  // Get last 3 items (the grid may have default items, in addition to the ones
  // installed by the test).
  std::vector<std::string> top_level_id_list =
      app_list_test_api_.GetTopLevelViewIdList();
  ASSERT_GT(top_level_id_list.size(), 2u);
  std::vector<std::string> trailing_items = {
      top_level_id_list[top_level_id_list.size() - 3],
      top_level_id_list[top_level_id_list.size() - 2],
      top_level_id_list[top_level_id_list.size() - 1],
  };

  EXPECT_EQ(std::vector<std::string>({app1_id, app3_id, app2_id}),
            trailing_items);
}

IN_PROC_BROWSER_TEST_F(ChromeAppListModelUpdaterProductivityLauncherTest,
                       IsNewInstall) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);

  // The built-in "Web Store" app is not a new install.
  ChromeAppListItem* web_store_item =
      model_updater->FindItem(extensions::kWebStoreAppId);
  ASSERT_TRUE(web_store_item);
  EXPECT_FALSE(web_store_item->CloneMetadata()->is_new_install);

  // Install 2 apps.
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Both apps are new installs.
  ChromeAppListItem* item1 = model_updater->FindItem(app1_id);
  ASSERT_TRUE(item1);
  EXPECT_TRUE(item1->CloneMetadata()->is_new_install);

  ChromeAppListItem* item2 = model_updater->FindItem(app2_id);
  ASSERT_TRUE(item2);
  EXPECT_TRUE(item2->CloneMetadata()->is_new_install);

  // Launch the first app.
  item1->Activate(ui::EF_NONE);

  // First app is no longer a new install.
  EXPECT_FALSE(item1->CloneMetadata()->is_new_install);

  // Second app is still a new install.
  EXPECT_TRUE(item2->CloneMetadata()->is_new_install);
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       PRE_PersistTrailingUserCreatedPage) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Create the app list view and show the apps grid.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  ASSERT_EQ(1, app_list_test_api_.GetPaginationModel()->total_pages());

  app_list_test_api_.GetLastItemInAppsGridView()->RequestFocus();

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::VKEY_DOWN, /*control=*/true, /*shift=*/false,
      /*alt=*/false, /*command=*/false));
  EXPECT_EQ(2, app_list_test_api_.GetPaginationModel()->total_pages());
}

IN_PROC_BROWSER_TEST_F(AppPositionReorderingTest,
                       PersistTrailingUserCreatedPage) {
  const std::string app1_id =
      LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
  ASSERT_FALSE(app1_id.empty());
  const std::string app2_id =
      LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
  ASSERT_FALSE(app2_id.empty());

  // Verify that the app list still has 2 pages after session restart.
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      ash::TOGGLE_APP_LIST_FULLSCREEN, {});
  EXPECT_EQ(2, app_list_test_api_.GetPaginationModel()->total_pages());
}
