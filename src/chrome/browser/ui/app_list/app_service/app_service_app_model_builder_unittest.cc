// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_service/app_service_app_model_builder.h"

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/app_service/public/mojom/types.mojom-shared.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"

using crostini::CrostiniTestHelper;
using extensions::AppSorting;
using extensions::ExtensionSystem;
using ::testing::_;
using ::testing::Matcher;

namespace {

const size_t kDefaultAppCount = 3u;

constexpr char kRootFolderName[] = "Linux apps";
constexpr char kDummyApp1Name[] = "dummy1";
constexpr char kDummyApp2Id[] = "dummy2";
constexpr char kDummyApp2Name[] = "dummy2";
constexpr char kAppNewName[] = "new name";
constexpr char kBananaAppId[] = "banana";
constexpr char kBananaAppName[] = "banana app name";

// Convenience matcher for some important fields of the chrome app.
MATCHER_P3(IsChromeApp, id, name, folder_id, "") {
  Matcher<std::string> id_m(id);
  Matcher<std::string> name_m(name);
  Matcher<std::string> folder_id_m(folder_id);
  return id_m.Matches(arg->id()) && name_m.Matches(arg->name()) &&
         folder_id_m.Matches(arg->folder_id());
}

// Matches a chrome app item if its persistence field is set to true.
MATCHER(IsPersistentApp, "") {
  return arg->is_persistent();
}

// Get a set of all apps in |model|.
std::vector<std::string> GetModelContent(AppListModelUpdater* model_updater) {
  std::vector<std::string> content;
  for (size_t i = 0; i < model_updater->ItemCount(); ++i)
    content.push_back(model_updater->ItemAtForTest(i)->name());
  return content;
}

scoped_refptr<extensions::Extension> MakeApp(const std::string& name,
                                             const std::string& version,
                                             const std::string& url,
                                             const std::string& id) {
  std::string err;
  base::DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", version);
  value.SetString("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::Manifest::INTERNAL, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

// For testing purposes, we want to pretend there are only |app_type| apps on
// the system. This method removes the others.
void RemoveApps(apps::mojom::AppType app_type,
                Profile* profile,
                FakeAppListModelUpdater* model_updater) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  DCHECK(proxy);
  proxy->FlushMojoCallsForTesting();
  proxy->AppRegistryCache().ForEachApp(
      [&model_updater, &app_type](const apps::AppUpdate& update) {
        if (update.AppType() != app_type) {
          model_updater->RemoveItem(update.AppId());
        }
      });
}

}  // namespace

class AppServiceAppModelBuilderTest : public AppListTestBase {
 public:
  AppServiceAppModelBuilderTest() {}
  ~AppServiceAppModelBuilderTest() override {}

  AppServiceAppModelBuilderTest(const AppServiceAppModelBuilderTest&) = delete;
  AppServiceAppModelBuilderTest& operator=(
      const AppServiceAppModelBuilderTest&) = delete;

  void TearDown() override {
    ResetBuilder();
    AppListTestBase::TearDown();
  }

 protected:
  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  // Creates a new builder, destroying any existing one.
  void CreateBuilder(bool guest_mode) {
    ResetBuilder();  // Destroy any existing builder in the correct order.

    app_service_test_.UninstallAllApps(profile());
    testing_profile()->SetGuestSession(guest_mode);
    app_service_test_.SetUp(testing_profile());
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<AppServiceAppModelBuilder>(controller_.get());
    builder_->Initialize(nullptr, testing_profile(), model_updater_.get());
  }

  apps::AppServiceTest app_service_test_;
  std::unique_ptr<AppServiceAppModelBuilder> builder_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
};

class BuiltInAppTest : public AppServiceAppModelBuilderTest {
 protected:
  // Creates a new builder, destroying any existing one.
  void CreateBuilder(bool guest_mode) {
    AppServiceAppModelBuilderTest::CreateBuilder(guest_mode);
    RemoveApps(apps::mojom::AppType::kBuiltIn, testing_profile(),
               model_updater_.get());
  }
};

class ExtensionAppTest : public AppServiceAppModelBuilderTest {
 public:
  void SetUp() override {
    AppListTestBase::SetUp();

    default_apps_ = {"Hosted App", "Packaged App 1", "Packaged App 2"};
    CreateBuilder();
  }

 protected:
  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    AppServiceAppModelBuilderTest::CreateBuilder(false /*guest_mode*/);
    RemoveApps(apps::mojom::AppType::kExtension, testing_profile(),
               model_updater_.get());
  }

  std::vector<std::string> default_apps_;
};

TEST_F(BuiltInAppTest, Build) {
  // The internal apps list is provided by GetInternalAppList() in
  // internal_app_metadata.cc. Only count the apps can display in launcher.
  std::string built_in_apps_name;
  CreateBuilder(false);
  EXPECT_EQ(app_list::GetNumberOfInternalAppsShowInLauncherForTest(
                &built_in_apps_name, profile()),
            model_updater_->ItemCount());
  EXPECT_EQ(built_in_apps_name,
            base::JoinString(GetModelContent(model_updater_.get()), ","));
}

TEST_F(BuiltInAppTest, BuildGuestMode) {
  // The internal apps list is provided by GetInternalAppList() in
  // internal_app_metadata.cc. Only count the apps can display in launcher.
  std::string built_in_apps_name;
  CreateBuilder(true);
  EXPECT_EQ(app_list::GetNumberOfInternalAppsShowInLauncherForTest(
                &built_in_apps_name, profile()),
            model_updater_->ItemCount());
  EXPECT_EQ(built_in_apps_name,
            base::JoinString(GetModelContent(model_updater_.get()), ","));
}

TEST_F(ExtensionAppTest, Build) {
  // The apps list would have 3 extension apps in the profile.
  EXPECT_EQ(kDefaultAppCount, model_updater_->ItemCount());
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, HideWebStore) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", "0.0", "http://google.com",
              std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  // Install an "enterprise web store" app.
  scoped_refptr<extensions::Extension> enterprise_store =
      MakeApp("enterprise_webstore", "0.0", "http://google.com",
              std::string(extension_misc::kEnterpriseWebStoreAppId));
  service_->AddExtension(enterprise_store.get());

  app_service_test_.SetUp(profile_.get());

  // Web stores should be present in the model.
  FakeAppListModelUpdater model_updater1;
  AppServiceAppModelBuilder builder1(controller_.get());
  builder1.Initialize(nullptr, profile_.get(), &model_updater1);
  EXPECT_TRUE(model_updater1.FindItem(store->id()));
  EXPECT_TRUE(model_updater1.FindItem(enterprise_store->id()));

  // Activate the HideWebStoreIcon policy.
  profile_->GetPrefs()->SetBoolean(prefs::kHideWebStoreIcon, true);
  app_service_test_.FlushMojoCalls();
  // Now the web stores should not be present anymore.
  EXPECT_FALSE(model_updater1.FindItem(store->id()));
  EXPECT_FALSE(model_updater1.FindItem(enterprise_store->id()));

  // Build a new model; web stores should NOT be present.
  FakeAppListModelUpdater model_updater2;
  AppServiceAppModelBuilder builder2(controller_.get());
  builder2.Initialize(nullptr, profile_.get(), &model_updater2);
  app_service_test_.FlushMojoCalls();
  EXPECT_FALSE(model_updater2.FindItem(store->id()));
  EXPECT_FALSE(model_updater2.FindItem(enterprise_store->id()));

  // Deactivate the HideWebStoreIcon policy again.
  profile_->GetPrefs()->SetBoolean(prefs::kHideWebStoreIcon, false);
  app_service_test_.FlushMojoCalls();
  // Now the web stores should have appeared.
  EXPECT_TRUE(model_updater2.FindItem(store->id()));
  EXPECT_TRUE(model_updater2.FindItem(enterprise_store->id()));
}

TEST_F(ExtensionAppTest, DisableAndEnable) {
  service_->DisableExtension(kHostedAppId,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  app_service_test_.FlushMojoCalls();
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));

  service_->EnableExtension(kHostedAppId);
  app_service_test_.FlushMojoCalls();
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, Uninstall) {
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  app_service_test_.FlushMojoCalls();
  EXPECT_EQ((std::vector<std::string>{"Hosted App", "Packaged App 1"}),
            GetModelContent(model_updater_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppTest, UninstallTerminatedApp) {
  ASSERT_NE(nullptr, registry()->GetInstalledExtension(kPackagedApp2Id));

  // Simulate an app termination.
  service_->TerminateExtension(kPackagedApp2Id);

  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  app_service_test_.FlushMojoCalls();
  EXPECT_EQ((std::vector<std::string>{"Hosted App", "Packaged App 1"}),
            GetModelContent(model_updater_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppTest, Reinstall) {
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));

  // Install kPackagedApp1Id again should not create a new entry.
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_.get());
  extensions::InstallObserver::ExtensionInstallParams params(
      kPackagedApp1Id, "", gfx::ImageSkia(), true, true);
  tracker->OnBeginExtensionInstall(params);
  app_service_test_.FlushMojoCalls();

  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, OrdinalPrefsChange) {
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();

  syncer::StringOrdinal package_app_page =
      sorting->GetPageOrdinal(kPackagedApp1Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page.CreateBefore());
  app_service_test_.FlushMojoCalls();
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));

  syncer::StringOrdinal app1_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp1Id);
  syncer::StringOrdinal app2_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp2Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page);
  sorting->SetAppLaunchOrdinal(kHostedAppId,
                               app1_ordinal.CreateBetween(app2_ordinal));
  app_service_test_.FlushMojoCalls();
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, OnExtensionMoved) {
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();
  sorting->SetPageOrdinal(kHostedAppId,
                          sorting->GetPageOrdinal(kPackagedApp1Id));

  sorting->OnExtensionMoved(kHostedAppId, kPackagedApp1Id, kPackagedApp2Id);
  app_service_test_.FlushMojoCalls();
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));

  sorting->OnExtensionMoved(kHostedAppId, kPackagedApp2Id, std::string());
  app_service_test_.FlushMojoCalls();
  // Old behavior: This would be restored to the default order.
  // New behavior: Sorting order still doesn't change.
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));

  sorting->OnExtensionMoved(kHostedAppId, std::string(), kPackagedApp1Id);
  app_service_test_.FlushMojoCalls();
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(default_apps_, GetModelContent(model_updater_.get()));
}

TEST_F(ExtensionAppTest, InvalidOrdinal) {
  // Creates a no-ordinal case.
  AppSorting* sorting = ExtensionSystem::Get(profile_.get())->app_sorting();
  sorting->ClearOrdinals(kPackagedApp1Id);

  // Creates a corrupted ordinal case.
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());
  prefs->UpdateExtensionPref(
      kHostedAppId, "page_ordinal",
      std::make_unique<base::Value>("a corrupted ordinal"));

  // This should not assert or crash.
  CreateBuilder();
}

// This test adds a bookmark app to the app list.
TEST_F(ExtensionAppTest, BookmarkApp) {
  const std::string kAppName = "Bookmark App";
  const std::string kAppVersion = "2014.1.24.19748";
  const std::string kAppUrl = "http://google.com";
  const std::string kAppId = "podhdnefolignjhecmjkbimfgioanahm";
  std::string err;
  base::DictionaryValue value;
  value.SetString("name", kAppName);
  value.SetString("version", kAppVersion);
  value.SetString("app.launch.web_url", kAppUrl);
  scoped_refptr<extensions::Extension> bookmark_app =
      extensions::Extension::Create(
          base::FilePath(), extensions::Manifest::INTERNAL, value,
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT |
              extensions::Extension::FROM_BOOKMARK,
          kAppId, &err);
  EXPECT_TRUE(err.empty());

  service_->AddExtension(bookmark_app.get());
  app_service_test_.SetUp(profile_.get());
  RemoveApps(apps::mojom::AppType::kWeb, profile_.get(), model_updater_.get());
  EXPECT_EQ(1u, model_updater_->ItemCount());
  EXPECT_EQ((std::vector<std::string>{kAppName}),
            GetModelContent(model_updater_.get()));
}

class CrostiniAppTest : public AppListTestBase {
 public:
  CrostiniAppTest() {}
  ~CrostiniAppTest() override {}

  CrostiniAppTest(const CrostiniAppTest&) = delete;
  CrostiniAppTest& operator=(const CrostiniAppTest&) = delete;

  void SetUp() override {
    AppListTestBase::SetUp();
    test_helper_ = std::make_unique<CrostiniTestHelper>(testing_profile());
    test_helper_->ReInitializeAppServiceIntegration();
    CreateBuilder();
  }

  void TearDown() override {
    ResetBuilder();
    test_helper_.reset();
    AppListTestBase::TearDown();
  }

 protected:
  AppListModelUpdater* GetModelUpdater() const {
    return sync_service_->GetModelUpdater();
  }

  size_t GetModelItemCount() const {
    // Pump the Mojo IPCs.
    base::RunLoop().RunUntilIdle();
    return sync_service_->GetModelUpdater()->ItemCount();
  }

  std::vector<ChromeAppListItem*> GetAllApps() const {
    std::vector<ChromeAppListItem*> result;
    for (size_t i = 0; i < GetModelItemCount(); ++i) {
      result.emplace_back(GetModelUpdater()->ItemAtForTest(i));
    }
    return result;
  }

  // For testing purposes, we want to pretend there are only crostini apps on
  // the system. This method removes the others.
  void RemoveNonCrostiniApps() {
    std::vector<std::string> existing_item_ids;
    for (const auto& pair : sync_service_->sync_items()) {
      existing_item_ids.emplace_back(pair.first);
    }
    for (const std::string& id : existing_item_ids) {
      if (id == crostini::kCrostiniFolderId ||
          id == crostini::kCrostiniTerminalId) {
        continue;
      }
      sync_service_->RemoveItem(id);
    }
  }

  void CreateBuilder() {
    model_updater_factory_scope_ = std::make_unique<
        app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>(
        base::BindRepeating(
            [](Profile* profile) -> std::unique_ptr<AppListModelUpdater> {
              return std::make_unique<FakeAppListModelUpdater>(profile);
            },
            profile()));
    // The AppListSyncableService creates the CrostiniAppModelBuilder.
    sync_service_ =
        std::make_unique<app_list::AppListSyncableService>(profile_.get());
    RemoveNonCrostiniApps();
  }

  void ResetBuilder() {
    sync_service_.reset();
    model_updater_factory_scope_.reset();
  }

  crostini::CrostiniRegistryService* RegistryService() {
    return crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  }

  std::string TerminalAppName() {
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  }

  std::unique_ptr<app_list::AppListSyncableService> sync_service_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;

 private:
  std::unique_ptr<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>
      model_updater_factory_scope_;
};

// Test that the Terminal app is only shown when Crostini is enabled
TEST_F(CrostiniAppTest, EnableAndDisableCrostini) {
  // Reset things so we start with Crostini not enabled.
  ResetBuilder();
  test_helper_.reset();
  test_helper_ = std::make_unique<CrostiniTestHelper>(
      testing_profile(), /*enable_crostini=*/false);
  CreateBuilder();

  EXPECT_EQ(0u, GetModelItemCount());

  CrostiniTestHelper::EnableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId)));

  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_THAT(GetAllApps(), testing::IsEmpty());
}

TEST_F(CrostiniAppTest, AppInstallation) {
  // Terminal app.
  EXPECT_EQ(1u, GetModelItemCount());

  test_helper_->SetupDummyApps();

  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp1Name, crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, crostini::kCrostiniFolderId)));

  test_helper_->AddApp(
      CrostiniTestHelper::BasicApp(kBananaAppId, kBananaAppName));
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp1Name, crostini::kCrostiniFolderId),
                  IsChromeApp(_, kDummyApp2Name, crostini::kCrostiniFolderId),
                  IsChromeApp(_, kBananaAppName, crostini::kCrostiniFolderId)));
}

// Test that the app model builder correctly picks up changes to existing apps.
TEST_F(CrostiniAppTest, UpdateApps) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // Setting NoDisplay to true should hide an app.
  vm_tools::apps::App dummy1 = test_helper_->GetApp(0);
  dummy1.set_no_display(true);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, _, _),
                  IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name),
                              _, _)));

  // Setting NoDisplay to false should unhide an app.
  dummy1.set_no_display(false);
  test_helper_->AddApp(dummy1);
  EXPECT_THAT(
      GetAllApps(),
      testing::UnorderedElementsAre(
          IsChromeApp(crostini::kCrostiniTerminalId, _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name), _, _),
          IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name), _,
                      _)));

  // Changes to app names should be detected.
  vm_tools::apps::App dummy2 =
      CrostiniTestHelper::BasicApp(kDummyApp2Id, kAppNewName);
  test_helper_->AddApp(dummy2);
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, _, _),
                  IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp1Name),
                              kDummyApp1Name, _),
                  IsChromeApp(CrostiniTestHelper::GenerateAppId(kDummyApp2Name),
                              kAppNewName, _)));
}

// Test that the app model builder handles removed apps
TEST_F(CrostiniAppTest, RemoveApps) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // Remove dummy1
  test_helper_->RemoveApp(0);
  EXPECT_EQ(2u, GetModelItemCount());

  // Remove dummy2
  test_helper_->RemoveApp(0);
  EXPECT_EQ(1u, GetModelItemCount());
}

// Tests that the crostini folder is (re)created with the correct parameters.
TEST_F(CrostiniAppTest, CreatesFolder) {
  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId)));

  // We simulate ash creating the crostini folder and calling back into chrome
  // (rather than use a full browser test).
  auto metadata = std::make_unique<ash::AppListItemMetadata>();
  metadata->id = crostini::kCrostiniFolderId;
  GetModelUpdater()->OnFolderCreated(std::move(metadata));

  EXPECT_THAT(GetAllApps(),
              testing::UnorderedElementsAre(
                  IsChromeApp(crostini::kCrostiniTerminalId, TerminalAppName(),
                              crostini::kCrostiniFolderId),
                  testing::AllOf(IsChromeApp(crostini::kCrostiniFolderId,
                                             kRootFolderName, ""),
                                 IsPersistentApp())));
}

// Test that the Terminal app is removed when Crostini is disabled.
TEST_F(CrostiniAppTest, DisableCrostini) {
  test_helper_->SetupDummyApps();
  // 3 apps.
  EXPECT_EQ(3u, GetModelItemCount());

  // The uninstall flow removes all apps before setting the CrostiniEnabled pref
  // to false, so we need to do that explicitly too.
  RegistryService()->ClearApplicationList(crostini::kCrostiniDefaultVmName, "");
  CrostiniTestHelper::DisableCrostini(testing_profile());
  EXPECT_EQ(0u, GetModelItemCount());
}
