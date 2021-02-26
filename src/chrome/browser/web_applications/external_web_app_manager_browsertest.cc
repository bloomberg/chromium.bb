// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_manager.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class ExternalWebAppManagerBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExternalWebAppManagerBrowserTest() {
    ExternalWebAppManager::SkipStartupForTesting();
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    os_hooks_suppress_ =
        OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  GURL GetAppUrl() const {
    return embedded_test_server()->GetURL("/web_apps/basic.html");
  }

  const AppRegistrar& registrar() {
    return WebAppProvider::Get(browser()->profile())->registrar();
  }

  void SyncEmptyConfigs() {
    std::vector<base::Value> app_configs;
    ExternalWebAppManager::SetConfigsForTesting(&app_configs);

    base::RunLoop run_loop;
    WebAppProvider::Get(browser()->profile())
        ->external_web_app_manager_for_testing()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, InstallResultCode> install_results,
                std::map<GURL, bool> uninstall_results) {
              EXPECT_EQ(install_results.size(), 0u);
              EXPECT_EQ(uninstall_results.size(), 0u);
              run_loop.Quit();
            }));
    run_loop.Run();

    ExternalWebAppManager::SetConfigsForTesting(nullptr);
  }

  // Mocks "icon.png" as available in the config's directory.
  base::Optional<InstallResultCode> SyncDefaultAppConfig(
      const GURL& install_url,
      base::StringPiece app_config_string) {
    base::FilePath test_config_dir(FILE_PATH_LITERAL("test_dir"));
    ExternalWebAppManager::SetConfigDirForTesting(&test_config_dir);

    base::FilePath source_root_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir));
    base::FilePath test_icon_path =
        source_root_dir.Append(GetChromeTestDataDir())
            .AppendASCII("web_apps/blue-192.png");
    TestFileUtils file_utils(
        {{base::FilePath(FILE_PATH_LITERAL("test_dir/icon.png")),
          test_icon_path}});
    ExternalWebAppManager::SetFileUtilsForTesting(&file_utils);

    std::vector<base::Value> app_configs;
    app_configs.push_back(*base::JSONReader::Read(app_config_string));
    ExternalWebAppManager::SetConfigsForTesting(&app_configs);

    base::Optional<InstallResultCode> code;
    base::RunLoop sync_run_loop;
    WebAppProvider::Get(browser()->profile())
        ->external_web_app_manager_for_testing()
        .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
            [&](std::map<GURL, InstallResultCode> install_results,
                std::map<GURL, bool> uninstall_results) {
              auto it = install_results.find(install_url);
              if (it != install_results.end())
                code = it->second;
              sync_run_loop.Quit();
            }));
    sync_run_loop.Run();

    ExternalWebAppManager::SetConfigDirForTesting(nullptr);
    ExternalWebAppManager::SetFileUtilsForTesting(nullptr);
    ExternalWebAppManager::SetConfigsForTesting(nullptr);

    return code;
  }

  ~ExternalWebAppManagerBrowserTest() override = default;

 private:
  ScopedOsHooksSuppress os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       LaunchQueryParamsBasic) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = GenerateAppIdFromURL(start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(SyncDefaultAppConfig(start_url, base::ReplaceStringPlaceholders(
                                                R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "launch_query_params": "test_launch_params"
              })",
                                                {start_url.spec()}, nullptr)),
            InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url =
      embedded_test_server()->GetURL("/web_apps/basic.html?test_launch_params");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       LaunchQueryParamsDuplicate) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  AppId app_id = GenerateAppIdFromURL(start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(
      SyncDefaultAppConfig(install_url, base::ReplaceStringPlaceholders(
                                            R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "launch_query_params": "query_params=in"
              })",
                                            {install_url.spec()}, nullptr)),
      InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  // We should not duplicate the query param if start_url already has it.
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), start_url);

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      start_url);
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       LaunchQueryParamsComplex) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html");
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/query_params_in_start_url.html?query_params=in&start=url");
  AppId app_id = GenerateAppIdFromURL(start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(
      SyncDefaultAppConfig(install_url, base::ReplaceStringPlaceholders(
                                            R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "launch_query_params": "!@#$$%^*&)("
              })",
                                            {install_url.spec()}, nullptr)),
      InstallResultCode::kSuccessNewInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);

  GURL launch_url = embedded_test_server()->GetURL(
      "/web_apps/"
      "query_params_in_start_url.html?query_params=in&start=url&!@%23$%^*&)(");
  EXPECT_EQ(registrar().GetAppLaunchUrl(app_id), launch_url);

  Browser* app_browser = LaunchWebAppBrowserAndWait(profile(), app_id);
  EXPECT_EQ(
      app_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      launch_url);
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest, UninstallAndReplace) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* profile = browser()->profile();

  // Install Chrome app to be replaced.
  const char kChromeAppDirectory[] = "app";
  const char kChromeAppName[] = "App Test";
  const extensions::Extension* app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII(kChromeAppDirectory), 1,
      extensions::Manifest::INTERNAL, extensions::Extension::NO_FLAGS);
  EXPECT_EQ(app->name(), kChromeAppName);

  // Start listening for Chrome app uninstall.
  extensions::TestExtensionRegistryObserver uninstall_observer(
      extensions::ExtensionRegistry::Get(profile));

  EXPECT_EQ(SyncDefaultAppConfig(GetAppUrl(),
                                 base::ReplaceStringPlaceholders(
                                     R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "uninstall_and_replace": ["$2"]
              })",
                                     {GetAppUrl().spec(), app->id()}, nullptr)),
            InstallResultCode::kSuccessNewInstall);

  // Chrome app should get uninstalled.
  scoped_refptr<const extensions::Extension> uninstalled_app =
      uninstall_observer.WaitForExtensionUninstalled();
  EXPECT_EQ(app, uninstalled_app.get());
}

// The offline manifest JSON config functionality is only available on Chrome
// OS.
#if defined(OS_CHROMEOS)

// Check that offline fallback installs work offline.
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OfflineFallbackManifestSiteOffline) {
  constexpr char kAppInstallUrl[] = "https://offline-site.com/install.html";
  constexpr char kAppName[] = "Offline app name";
  constexpr char kAppStartUrl[] = "https://offline-site.com/start.html";
  constexpr char kAppScope[] = "https://offline-site.com/";

  AppId app_id = GenerateAppIdFromURL(GURL(kAppStartUrl));
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(
      SyncDefaultAppConfig(
          GURL(kAppInstallUrl),
          base::ReplaceStringPlaceholders(
              R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "offline_manifest": {
                  "name": "$2",
                  "start_url": "$3",
                  "scope": "$4",
                  "display": "minimal-ui",
                  "theme_color_argb_hex": "AABBCCDD",
                  "icon_any_pngs": ["icon.png"]
                }
              })",
              {kAppInstallUrl, kAppName, kAppStartUrl, kAppScope}, nullptr)),
      InstallResultCode::kSuccessOfflineFallbackInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), kAppStartUrl);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), kAppScope);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLUE);
}

// Check that offline fallback installs attempt fetching the install_url.
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OfflineFallbackManifestSiteOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This install_url serves a manifest with different values to what we specify
  // in the offline_manifest. Check that it gets used instead of the
  // offline_manifest.
  GURL install_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  GURL offline_start_url = embedded_test_server()->GetURL(
      "/web_apps/offline-only-start-url-that-does-not-exist.html");
  GURL scope = embedded_test_server()->GetURL("/web_apps/");

  AppId offline_app_id = GenerateAppIdFromURL(offline_start_url);
  EXPECT_FALSE(registrar().IsInstalled(offline_app_id));

  EXPECT_EQ(SyncDefaultAppConfig(
                install_url, base::ReplaceStringPlaceholders(
                                 R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "offline_manifest": {
                  "name": "Offline only app name",
                  "start_url": "$2",
                  "scope": "$3",
                  "display": "minimal-ui",
                  "theme_color_argb_hex": "AABBCCDD",
                  "icon_any_pngs": ["icon.png"]
                }
              })",
                                 {install_url.spec(), offline_start_url.spec(),
                                  scope.spec()},
                                 nullptr)),
            InstallResultCode::kSuccessNewInstall);

  EXPECT_FALSE(registrar().IsInstalled(offline_app_id));

  // basic.html's manifest start_url is basic.html.
  AppId app_id = GenerateAppIdFromURL(install_url);
  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), "Basic web app");
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), install_url);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), scope);
}

// Check that offline only installs work offline.
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OfflineOnlyManifestSiteOffline) {
  constexpr char kAppInstallUrl[] = "https://offline-site.com/install.html";
  constexpr char kAppName[] = "Offline app name";
  constexpr char kAppStartUrl[] = "https://offline-site.com/start.html";
  constexpr char kAppScope[] = "https://offline-site.com/";

  AppId app_id = GenerateAppIdFromURL(GURL(kAppStartUrl));
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(
      SyncDefaultAppConfig(
          GURL(kAppInstallUrl),
          base::ReplaceStringPlaceholders(
              R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "only_use_offline_manifest": true,
                "offline_manifest": {
                  "name": "$2",
                  "start_url": "$3",
                  "scope": "$4",
                  "display": "minimal-ui",
                  "theme_color_argb_hex": "AABBCCDD",
                  "icon_any_pngs": ["icon.png"]
                }
              })",
              {kAppInstallUrl, kAppName, kAppStartUrl, kAppScope}, nullptr)),
      InstallResultCode::kSuccessOfflineOnlyInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), kAppStartUrl);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), kAppScope);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLUE);
}

// Check that offline only installs don't fetch from the install_url.
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OfflineOnlyManifestSiteOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // This install_url serves a manifest with different values to what we specify
  // in the offline_manifest. Check that it doesn't get used.
  GURL install_url = GetAppUrl();
  const char kAppName[] = "Offline only app name";
  GURL start_url = embedded_test_server()->GetURL(
      "/web_apps/offline-only-start-url-that-does-not-exist.html");
  GURL scope = embedded_test_server()->GetURL("/web_apps/");

  AppId app_id = GenerateAppIdFromURL(start_url);
  EXPECT_FALSE(registrar().IsInstalled(app_id));

  EXPECT_EQ(
      SyncDefaultAppConfig(install_url, base::ReplaceStringPlaceholders(
                                            R"({
                "app_url": "$1",
                "launch_container": "window",
                "user_type": ["unmanaged"],
                "only_use_offline_manifest": true,
                "offline_manifest": {
                  "name": "$2",
                  "start_url": "$3",
                  "scope": "$4",
                  "display": "minimal-ui",
                  "theme_color_argb_hex": "AABBCCDD",
                  "icon_any_pngs": ["icon.png"]
                }
              })",
                                            {install_url.spec(), kAppName,
                                             start_url.spec(), scope.spec()},
                                            nullptr)),
      InstallResultCode::kSuccessOfflineOnlyInstall);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_EQ(registrar().GetAppShortName(app_id), kAppName);
  EXPECT_EQ(registrar().GetAppStartUrl(app_id).spec(), start_url);
  EXPECT_EQ(registrar().GetAppScope(app_id).spec(), scope);
  // theme_color must be installed opaque.
  EXPECT_EQ(registrar().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(ReadAppIconPixel(browser()->profile(), app_id, /*size=*/192,
                             /*x=*/0, /*y=*/0),
            SK_ColorBLUE);
}

const char kOnlyForNewUsersInstallUrl[] = "https://example.org/";
const char kOnlyForNewUsersConfig[] = R"({
    "app_url": "https://example.org/",
    "launch_container": "window",
    "user_type": ["unmanaged"],
    "only_for_new_users": true,
    "only_use_offline_manifest": true,
    "offline_manifest": {
      "name": "Test",
      "start_url": "https://example.org/",
      "scope": "https://example.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  })";

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       PRE_OnlyForNewUsersWithNewUser) {
  // New user should have the app installed.
  EXPECT_EQ(SyncDefaultAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                 kOnlyForNewUsersConfig),
            InstallResultCode::kSuccessOfflineOnlyInstall);
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OnlyForNewUsersWithNewUser) {
  // App should persist after user stops being a new user.
  EXPECT_EQ(SyncDefaultAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                 kOnlyForNewUsersConfig),
            InstallResultCode::kSuccessAlreadyInstalled);
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       PRE_OnlyForNewUsersWithOldUser) {
  // Simulate running Chrome without the configs present.
  SyncEmptyConfigs();
}
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest,
                       OnlyForNewUsersWithOldUser) {
  // This instance of Chrome should be considered not a new user after the
  // previous PRE_ launch and sync.
  EXPECT_EQ(SyncDefaultAppConfig(GURL(kOnlyForNewUsersInstallUrl),
                                 kOnlyForNewUsersConfig),
            base::nullopt);
}

#endif  // defined(OS_CHROMEOS)

// Icon resourcs are only available on Chrome branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(ExternalWebAppManagerBrowserTest, PreinstalledWebApps) {
  base::AutoReset<bool> scope =
      SetExternalAppInstallFeatureAlwaysEnabledForTesting();

  constexpr std::array<const char*, 1> kExpectedInstallUrls = {
      "https://docs.google.com/document/installwebapp?usp=chrome_default",
  };

  base::RunLoop run_loop;
  WebAppProvider::Get(browser()->profile())
      ->external_web_app_manager_for_testing()
      .LoadAndSynchronizeForTesting(base::BindLambdaForTesting(
          [&](std::map<GURL, InstallResultCode> install_results,
              std::map<GURL, bool> uninstall_results) {
            EXPECT_EQ(install_results.size(), kExpectedInstallUrls.size());
            for (const char* install_url : kExpectedInstallUrls) {
              EXPECT_TRUE(base::Contains(install_results, GURL(install_url)))
                  << install_url;
            }

            EXPECT_EQ(uninstall_results.size(), 0u);
            run_loop.Quit();
          }));
  run_loop.Run();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace web_app
