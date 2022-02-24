// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"
#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

const base::Time kLastLaunchTime = base::Time::Now();
const base::Time kInstallTime = base::Time::Now();
const char kUrl[] = "https://example.com/";

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
scoped_refptr<extensions::Extension> MakeExtensionApp(
    const std::string& name,
    const std::string& version,
    const std::string& url,
    const std::string& id) {
  std::string err;
  base::DictionaryValue value;
  value.SetStringKey("name", name);
  value.SetStringKey("version", version);
  value.SetStringPath("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AddArcPackage(ArcAppTest& arc_test,
                   const std::vector<arc::mojom::AppInfoPtr>& fake_apps) {
  for (const auto& fake_app : fake_apps) {
    base::flat_map<arc::mojom::AppPermission, arc::mojom::PermissionStatePtr>
        permissions;
    permissions.emplace(arc::mojom::AppPermission::CAMERA,
                        arc::mojom::PermissionState::New(/*granted=*/false,
                                                         /*managed=*/false));
    permissions.emplace(arc::mojom::AppPermission::LOCATION,
                        arc::mojom::PermissionState::New(/*granted=*/true,
                                                         /*managed=*/false));
    arc::mojom::ArcPackageInfoPtr package = arc::mojom::ArcPackageInfo::New(
        fake_app->package_name, /*package_version=*/1,
        /*last_backup_android_id=*/1,
        /*last_backup_time=*/1, /*sync=*/true, /*system=*/false,
        /*vpn_provider=*/false, /*web_app_info=*/nullptr, absl::nullopt,
        std::move(permissions));
    arc_test.AddPackage(package->Clone());
    arc_test.app_instance()->SendPackageAdded(package->Clone());
  }
}

apps::AppPtr MakeApp(apps::AppType app_type,
                     const std::string& app_id,
                     const std::string& name,
                     apps::Readiness readiness) {
  auto app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = readiness;
  app->name = name;
  app->short_name = name;
  app->install_reason = apps::InstallReason::kUser;
  app->install_source = apps::InstallSource::kSync;
  app->icon_key = apps::IconKey(
      /*timeline=*/1, apps::IconKey::kInvalidResourceId,
      /*icon_effects=*/0);
  return app;
}

apps::Permissions MakeFakePermissions() {
  apps::Permissions permissions;
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kCamera,
      std::make_unique<apps::PermissionValue>(false),
      /*is_managed*/ false));
  permissions.push_back(std::make_unique<apps::Permission>(
      apps::PermissionType::kLocation,
      std::make_unique<apps::PermissionValue>(true),
      /*is_managed*/ false));
  return permissions;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsEqual(const apps::Permissions& source, const apps::Permissions& target) {
  if (source.size() != target.size()) {
    return false;
  }

  for (int i = 0; i < static_cast<int>(source.size()); i++) {
    if (*source[i] != *target[i]) {
      return false;
    }
  }
  return true;
}

apps::IntentFilters CreateIntentFilters() {
  const GURL url(kUrl);
  apps::IntentFilters filters;
  apps::IntentFilterPtr filter = std::make_unique<apps::IntentFilter>();

  apps::ConditionValues values1;
  values1.push_back(std::make_unique<apps::ConditionValue>(
      apps_util::kIntentActionView, apps::PatternMatchType::kNone));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kAction, std::move(values1)));

  apps::ConditionValues values2;
  values2.push_back(std::make_unique<apps::ConditionValue>(
      url.scheme(), apps::PatternMatchType::kNone));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(values2)));

  apps::ConditionValues values3;
  values3.push_back(std::make_unique<apps::ConditionValue>(
      url.host(), apps::PatternMatchType::kNone));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(values3)));

  apps::ConditionValues values4;
  values4.push_back(std::make_unique<apps::ConditionValue>(
      url.path(), apps::PatternMatchType::kPrefix));
  filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kPattern, std::move(values4)));

  filters.push_back(std::move(filter));

  return filters;
}

}  // namespace

namespace apps {

class PublisherTest : public extensions::ExtensionServiceTestBase {
 public:
  PublisherTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kAppServiceOnAppTypeInitializedWithoutMojom);
  }

  PublisherTest(const PublisherTest&) = delete;
  PublisherTest& operator=(const PublisherTest&) = delete;

  ~PublisherTest() override = default;

  // ExtensionServiceTestBase:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();
    ConfigureWebAppProvider();
  }

  void ConfigureWebAppProvider() {
    auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();

    auto externally_managed_app_manager =
        std::make_unique<web_app::ExternallyManagedAppManagerImpl>(profile());
    externally_managed_app_manager->SetUrlLoaderForTesting(
        std::move(url_loader));

    auto* const provider = web_app::FakeWebAppProvider::Get(profile());
    provider->SetExternallyManagedAppManager(
        std::move(externally_managed_app_manager));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    base::RunLoop().RunUntilIdle();
  }

  std::string CreateWebApp(const std::string& app_name) {
    const GURL kAppUrl(kUrl);

    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->title = base::UTF8ToUTF16(app_name);
    web_app_info->start_url = kAppUrl;
    web_app_info->scope = kAppUrl;
    web_app_info->user_display_mode = web_app::DisplayMode::kStandalone;

    return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void RemoveArcApp(const std::string& app_id) {
    ArcApps* arc_apps = ArcAppsFactory::GetForProfile(profile());
    ASSERT_TRUE(arc_apps);
    arc_apps->OnAppRemoved(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void VerifyOptionalBool(absl::optional<bool> source,
                          absl::optional<bool> target) {
    if (source.has_value()) {
      EXPECT_EQ(source, target);
    }
  }

  void VerifyApp(AppType app_type,
                 const std::string& app_id,
                 const std::string& name,
                 apps::Readiness readiness,
                 InstallReason install_reason,
                 InstallSource install_source,
                 const std::vector<std::string>& additional_search_terms,
                 base::Time last_launch_time,
                 base::Time install_time,
                 const apps::Permissions& permissions,
                 absl::optional<bool> is_platform_app = absl::nullopt,
                 absl::optional<bool> recommendable = absl::nullopt,
                 absl::optional<bool> searchable = absl::nullopt,
                 absl::optional<bool> show_in_launcher = absl::nullopt,
                 absl::optional<bool> show_in_shelf = absl::nullopt,
                 absl::optional<bool> show_in_search = absl::nullopt,
                 absl::optional<bool> show_in_management = absl::nullopt,
                 absl::optional<bool> handles_intents = absl::nullopt,
                 absl::optional<bool> allow_uninstall = absl::nullopt,
                 absl::optional<bool> has_badge = absl::nullopt,
                 absl::optional<bool> paused = absl::nullopt,
                 WindowMode window_mode = WindowMode::kUnknown) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();

    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(app_type, cache.states_[app_id]->app_type);
    ASSERT_TRUE(cache.states_[app_id]->name.has_value());
    EXPECT_EQ(name, cache.states_[app_id]->name.value());
    EXPECT_EQ(readiness, cache.states_[app_id]->readiness);
    ASSERT_TRUE(cache.states_[app_id]->icon_key.has_value());
    EXPECT_EQ(install_reason, cache.states_[app_id]->install_reason);
    EXPECT_EQ(install_source, cache.states_[app_id]->install_source);
    EXPECT_EQ(additional_search_terms,
              cache.states_[app_id]->additional_search_terms);
    if (!last_launch_time.is_null()) {
      EXPECT_EQ(last_launch_time, cache.states_[app_id]->last_launch_time);
    }
    if (!install_time.is_null()) {
      EXPECT_EQ(install_time, cache.states_[app_id]->install_time);
    }
    if (!permissions.empty()) {
      EXPECT_TRUE(IsEqual(permissions, cache.states_[app_id]->permissions));
    }
    VerifyOptionalBool(is_platform_app, cache.states_[app_id]->is_platform_app);
    VerifyOptionalBool(recommendable, cache.states_[app_id]->recommendable);
    VerifyOptionalBool(searchable, cache.states_[app_id]->searchable);
    VerifyOptionalBool(show_in_launcher,
                       cache.states_[app_id]->show_in_launcher);
    VerifyOptionalBool(show_in_shelf, cache.states_[app_id]->show_in_shelf);
    VerifyOptionalBool(show_in_search, cache.states_[app_id]->show_in_search);
    VerifyOptionalBool(show_in_management,
                       cache.states_[app_id]->show_in_management);
    VerifyOptionalBool(handles_intents, cache.states_[app_id]->handles_intents);
    VerifyOptionalBool(allow_uninstall, cache.states_[app_id]->allow_uninstall);
    VerifyOptionalBool(has_badge, cache.states_[app_id]->has_badge);
    VerifyOptionalBool(paused, cache.states_[app_id]->paused);
    if (window_mode != WindowMode::kUnknown) {
      EXPECT_EQ(window_mode, cache.states_[app_id]->window_mode);
    }
  }

  void VerifyAppIsRemoved(const std::string& app_id) {
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_NE(cache.states_.end(), cache.states_.find(app_id));
    EXPECT_EQ(apps::Readiness::kUninstalledByUser,
              cache.states_[app_id]->readiness);
  }

  void VerifyIntentFilters(const std::string& app_id) {
    apps::IntentFilters source = CreateIntentFilters();

    apps::IntentFilters target;
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->AppRegistryCache()
        .ForApp(app_id, [&target](const apps::AppUpdate& update) {
          target = update.GetIntentFilters();
        });

    EXPECT_EQ(source.size(), target.size());
    for (int i = 0; i < static_cast<int>(source.size()); i++) {
      EXPECT_EQ(*source[i], *target[i]);
    }
  }

  void VerifyAppTypeIsInitialized(AppType app_type) {
    // TODO(crbug.com/1253250): Remove FlushMojoCallsForTesting when
    // OnAppTypeInitialized doesn't check the mojom App struct.
    AppServiceProxyFactory::GetForProfile(profile())
        ->FlushMojoCallsForTesting();
    AppRegistryCache& cache =
        AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache();
    ASSERT_TRUE(cache.IsAppTypeInitialized(app_type));
    ASSERT_TRUE(base::Contains(cache.InitializedAppTypes(), app_type));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  raw_ptr<web_app::TestWebAppUrlLoader> url_loader_ = nullptr;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PublisherTest, ArcAppsOnApps) {
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  // Install fake apps.
  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());
  AddArcPackage(arc_test, arc_test.fake_apps());

  // Verify ARC apps are added to AppRegistryCache.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_TRUE(prefs);
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          apps::Permissions(), /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*has_badge=*/false, /*paused=*/false);
      // Simulate the app is removed.
      RemoveArcApp(app_id);
      VerifyAppIsRemoved(app_id);
    }
  }
  VerifyAppTypeIsInitialized(AppType::kArc);

  // Verify the initialization process again with a new ArcApps object.
  std::unique_ptr<ArcApps> arc_apps = std::make_unique<ArcApps>(
      AppServiceProxyFactory::GetForProfile(profile()));
  ASSERT_TRUE(arc_apps.get());
  arc_apps->Initialize();

  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          MakeFakePermissions(), /*is_platform_app=*/false,
          /*recommendable=*/true, /*searchable=*/true,
          /*show_in_launcher=*/true, /*show_in_shelf=*/true,
          /*show_in_search=*/true, /*show_in_management=*/true,
          /*handles_intents=*/true,
          /*allow_uninstall=*/app_info->ready && !app_info->sticky,
          /*has_badge=*/false, /*paused=*/false);

      // Test OnAppLastLaunchTimeUpdated.
      const base::Time before_time = base::Time::Now();
      prefs->SetLastLaunchTime(app_id);
      app_info = prefs->GetApp(app_id);
      EXPECT_GE(app_info->last_launch_time, before_time);
      VerifyApp(
          AppType::kArc, app_id, app_info->name, Readiness::kReady,
          app_info->sticky ? InstallReason::kSystem : InstallReason::kUser,
          app_info->sticky ? InstallSource::kSystem : InstallSource::kPlayStore,
          {}, app_info->last_launch_time, app_info->install_time,
          MakeFakePermissions());
    }
  }

  arc_apps->Shutdown();
}

TEST_F(PublisherTest, BuiltinAppsOnApps) {
  // Verify Builtin apps are added to AppRegistryCache.
  for (const auto& internal_app : app_list::GetInternalAppList(profile())) {
    if ((internal_app.app_id == nullptr) ||
        (internal_app.name_string_resource_id == 0) ||
        (internal_app.icon_resource_id <= 0)) {
      continue;
    }
    std::vector<std::string> additional_search_terms;
    if (internal_app.searchable_string_resource_id != 0) {
      additional_search_terms.push_back(
          l10n_util::GetStringUTF8(internal_app.searchable_string_resource_id));
    }
    VerifyApp(AppType::kBuiltIn, internal_app.app_id,
              l10n_util::GetStringUTF8(internal_app.name_string_resource_id),
              Readiness::kReady, InstallReason::kSystem, InstallSource::kSystem,
              additional_search_terms, base::Time(), base::Time(),
              apps::Permissions(), /*is_platform_app=*/false,
              internal_app.recommendable, internal_app.searchable,
              internal_app.show_in_launcher, internal_app.searchable,
              internal_app.searchable, /*show_in_management=*/false,
              internal_app.show_in_launcher, /*allow_uninstall=*/false);
  }
  VerifyAppTypeIsInitialized(AppType::kBuiltIn);
}

class StandaloneBrowserPublisherTest : public PublisherTest {
 public:
  StandaloneBrowserPublisherTest() : PublisherTest() {
    crosapi::browser_util::SetLacrosEnabledForTest(true);
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {features::kWebAppsCrosapi, chromeos::features::kLacrosPrimary,
         kAppServiceOnAppTypeInitializedWithoutMojom},
        {});
  }

  StandaloneBrowserPublisherTest(const StandaloneBrowserPublisherTest&) =
      delete;
  StandaloneBrowserPublisherTest& operator=(
      const StandaloneBrowserPublisherTest&) = delete;
  ~StandaloneBrowserPublisherTest() override = default;

  // PublisherTest:
  void SetUp() override {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail("testing_profile");
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    PublisherTest::SetUp();
  }

  void ExtensionAppsOnApps() {
    StandaloneBrowserExtensionApps* chrome_apps =
        StandaloneBrowserExtensionAppsFactory::GetForProfile(profile());
    std::vector<AppPtr> apps;
    auto app = MakeApp(AppType::kStandaloneBrowserChromeApp,
                       /*app_id=*/"a",
                       /*name=*/"TestApp", Readiness::kReady);
    app->is_platform_app = true;
    app->recommendable = false;
    app->searchable = false;
    app->show_in_launcher = false;
    app->show_in_shelf = false;
    app->show_in_search = false;
    app->show_in_management = false;
    app->handles_intents = false;
    app->allow_uninstall = false;
    app->has_badge = false;
    app->paused = false;
    apps.push_back(std::move(app));
    chrome_apps->OnApps(std::move(apps));
  }

  void WebAppsCrosapiOnApps() {
    WebAppsCrosapi* web_apps_crosapi =
        WebAppsCrosapiFactory::GetForProfile(profile());
    std::vector<AppPtr> apps;
    auto app = MakeApp(AppType::kWeb,
                       /*app_id=*/"a",
                       /*name=*/"TestApp", Readiness::kReady);
    app->additional_search_terms.push_back("TestApp");
    app->last_launch_time = kLastLaunchTime;
    app->install_time = kInstallTime;
    app->permissions = MakeFakePermissions();
    app->recommendable = true;
    app->searchable = true;
    app->show_in_launcher = true;
    app->show_in_shelf = true;
    app->show_in_search = true;
    app->show_in_management = true;
    app->handles_intents = true;
    app->allow_uninstall = true;
    app->has_badge = true;
    app->paused = true;
    app->window_mode = WindowMode::kBrowser;
    apps.push_back(std::move(app));
    web_apps_crosapi->OnApps(std::move(apps));
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserAppsOnApps) {
  VerifyApp(AppType::kStandaloneBrowser, app_constants::kLacrosAppId, "Lacros",
            Readiness::kReady, InstallReason::kSystem, InstallSource::kSystem,
            {"chrome"}, base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/false,
            /*recommendable=*/true, /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/false);
  VerifyAppTypeIsInitialized(AppType::kStandaloneBrowser);
}

TEST_F(StandaloneBrowserPublisherTest, StandaloneBrowserExtensionAppsOnApps) {
  ExtensionAppsOnApps();
  VerifyApp(AppType::kStandaloneBrowserChromeApp, "a", "TestApp",
            Readiness::kReady, InstallReason::kUser, InstallSource::kSync, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/true, /*recommendable=*/false,
            /*searchable=*/false,
            /*show_in_launcher=*/false, /*show_in_shelf=*/false,
            /*show_in_search=*/false, /*show_in_management=*/false,
            /*handles_intents=*/false, /*allow_uninstall=*/false,
            /*has_badge=*/false, /*paused=*/false);
}

TEST_F(StandaloneBrowserPublisherTest, WebAppsCrosapiOnApps) {
  WebAppsCrosapiOnApps();
  VerifyApp(AppType::kWeb, "a", "TestApp", Readiness::kReady,
            InstallReason::kUser, InstallSource::kSync, {"TestApp"},
            kLastLaunchTime, kInstallTime, MakeFakePermissions(),
            /*is_platform_app=*/absl::nullopt, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/true, /*paused=*/true, WindowMode::kBrowser);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(PublisherTest, ExtensionAppsOnApps) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeExtensionApp("webstore", "0.0", "http://google.com",
                       std::string(extensions::kWebStoreAppId));
  service_->AddExtension(store.get());

  // Re-init AppService to verify the init process.
  AppServiceTest app_service_test;
  app_service_test.SetUp(profile());
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/false, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);
  VerifyAppTypeIsInitialized(AppType::kChromeApp);

  // Uninstall the Chrome app.
  service_->UninstallExtension(
      store->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(),
            Readiness::kUninstalledByUser, InstallReason::kDefault,
            InstallSource::kChromeWebStore, {}, base::Time(), base::Time(),
            apps::Permissions(), /*is_platform_app=*/false,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);

  // Reinstall the Chrome app.
  service_->AddExtension(store.get());
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            base::Time(), base::Time(), apps::Permissions(),
            /*is_platform_app=*/false, /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false);

  // Test OnExtensionLastLaunchTimeChanged.
  extensions::ExtensionPrefs::Get(profile())->SetLastLaunchTime(
      store->id(), kLastLaunchTime);
  VerifyApp(AppType::kChromeApp, store->id(), store->name(), Readiness::kReady,
            InstallReason::kDefault, InstallSource::kChromeWebStore, {},
            kLastLaunchTime, base::Time(), apps::Permissions(),
            /*is_platform_app=*/false);
}

TEST_F(PublisherTest, WebAppsOnApps) {
  const std::string kAppName = "Web App";
  auto app_id = CreateWebApp(kAppName);
  AppServiceTest app_service_test_;
  app_service_test_.SetUp(profile());

  VerifyApp(AppType::kWeb, app_id, kAppName, Readiness::kReady,
            InstallReason::kSync, InstallSource::kBrowser, {}, base::Time(),
            base::Time(), apps::Permissions(), /*is_platform_app=*/false,
            /*recommendable=*/true,
            /*searchable=*/true,
            /*show_in_launcher=*/true, /*show_in_shelf=*/true,
            /*show_in_search=*/true, /*show_in_management=*/true,
            /*handles_intents=*/true, /*allow_uninstall=*/true,
            /*has_badge=*/false, /*paused=*/false, WindowMode::kWindow);
  VerifyIntentFilters(app_id);
  VerifyAppTypeIsInitialized(AppType::kWeb);
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps
