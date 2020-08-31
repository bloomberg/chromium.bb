// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_pending_app_manager.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const char kSettingsAppNameForLogging[] = "OSSettings";
const char kDiscoverAppNameForLogging[] = "Discover";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL AppUrl1() {
  return GURL(content::GetWebUIURL("system-app1"));
}
GURL AppUrl2() {
  return GURL(content::GetWebUIURL("system-app2"));
}
GURL AppUrl3() {
  return GURL(content::GetWebUIURL("system-app3"));
}

ExternalInstallOptions GetWindowedInstallOptions() {
  ExternalInstallOptions options(AppUrl1(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.add_to_search = true;
  options.add_to_management = false;
  options.is_disabled = false;
  options.bypass_service_worker_check = true;
  options.force_reinstall = true;
  return options;
}

}  // namespace

class SystemWebAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SystemWebAppManagerTest() {
    scoped_feature_list_.InitWithFeatures({features::kSystemWebApps}, {});
  }

  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = TestWebAppProvider::Get(profile());

    auto test_app_registrar = std::make_unique<TestAppRegistrar>();
    test_app_registrar_ = test_app_registrar.get();
    provider->SetRegistrar(std::move(test_app_registrar));

    auto test_pending_app_manager =
        std::make_unique<TestPendingAppManager>(test_app_registrar_);
    test_pending_app_manager_ = test_pending_app_manager.get();
    provider->SetPendingAppManager(std::move(test_pending_app_manager));

    auto test_file_handler_manager =
        std::make_unique<TestFileHandlerManager>(profile());
    test_file_handler_manager_ = test_file_handler_manager.get();
    provider->SetFileHandlerManager(std::move(test_file_handler_manager));

    auto system_web_app_manager =
        std::make_unique<TestSystemWebAppManager>(profile());
    system_web_app_manager_ = system_web_app_manager.get();
    provider->SetSystemWebAppManager(std::move(system_web_app_manager));

    auto ui_manager = std::make_unique<TestWebAppUiManager>();
    ui_manager_ = ui_manager.get();
    provider->SetWebAppUiManager(std::move(ui_manager));

    provider->Start();
  }

  void SimulatePreviouslyInstalledApp(GURL url,
                                      ExternalInstallSource install_source) {
    pending_app_manager()->SimulatePreviouslyInstalledApp(url, install_source);
  }

  bool IsInstalled(const GURL& install_url) {
    return test_app_registrar_->LookupExternalAppId(install_url).has_value();
  }

 protected:
  TestPendingAppManager* pending_app_manager() {
    return test_pending_app_manager_;
  }

  TestSystemWebAppManager* system_web_app_manager() {
    return system_web_app_manager_;
  }

  TestWebAppUiManager* ui_manager() { return ui_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppRegistrar* test_app_registrar_ = nullptr;
  TestPendingAppManager* test_pending_app_manager_ = nullptr;
  TestFileHandlerManager* test_file_handler_manager_ = nullptr;
  TestSystemWebAppManager* system_web_app_manager_ = nullptr;
  TestWebAppUiManager* ui_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerTest);
};

// Test that System Apps are uninstalled with the feature disabled.
TEST_F(SystemWebAppManagerTest, Disabled) {
  base::test::ScopedFeatureList disable_feature_list;
  disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

  SimulatePreviouslyInstalledApp(AppUrl1(),
                                 ExternalInstallSource::kSystemInstalled);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppNameForLogging, GURL(AppUrl1())));

  system_web_app_manager()->SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager()->Start();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pending_app_manager()->install_requests().empty());

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({AppUrl1()}),
            pending_app_manager()->uninstall_requests());
}

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppNameForLogging, AppUrl2()));

  system_web_app_manager()->SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager()->install_requests();
  EXPECT_FALSE(apps_to_install.empty());
}

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(AppUrl1(),
                                 ExternalInstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(AppUrl2(),
                                 ExternalInstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(AppUrl3(),
                                 ExternalInstallSource::kInternalDefault);
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));

  system_web_app_manager()->SetSystemAppsForTesting(std::move(system_apps));
  system_web_app_manager()->Start();

  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the System App list.
  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  EXPECT_EQ(pending_app_manager()->install_requests(),
            expected_install_options_list);

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({AppUrl2()}),
            pending_app_manager()->uninstall_requests());
}

TEST_F(SystemWebAppManagerTest, AlwaysUpdate) {
  system_web_app_manager()->SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);

  system_web_app_manager()->set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager()->Start();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, pending_app_manager()->install_requests().size());

  // Create another app. The version hasn't changed but the app should still
  // install.
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppNameForLogging, AppUrl2()));
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);
  system_web_app_manager()->Start();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, pending_app_manager()->install_requests().size());

  {
    // Disabling System Web Apps uninstalls without a version change.
    base::test::ScopedFeatureList disable_feature_list;
    disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

    system_web_app_manager()->Start();

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2u, pending_app_manager()->uninstall_requests().size());
  }

  // Re-enabling System Web Apps installs without a version change.
  system_web_app_manager()->Start();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, pending_app_manager()->install_requests().size());
}

TEST_F(SystemWebAppManagerTest, UpdateOnVersionChange) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager()->install_requests();

  system_web_app_manager()->SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);

  system_web_app_manager()->set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Create another app. The version hasn't changed, but we should immediately
  // install anyway, as if a user flipped a chrome://flag. The first app won't
  // force reinstall.
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppNameForLogging, AppUrl2()));
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, install_requests.size());
  EXPECT_FALSE(install_requests[1].force_reinstall);
  EXPECT_FALSE(install_requests[2].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager()->set_current_version(base::Version("2.0.0.0"));
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_TRUE(install_requests[4].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  {
    // Disabling System Web Apps uninstalls even without a version change.
    base::test::ScopedFeatureList disable_feature_list;
    disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

    system_web_app_manager()->Start();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(2u, pending_app_manager()->uninstall_requests().size());
    EXPECT_FALSE(IsInstalled(AppUrl1()));
    EXPECT_FALSE(IsInstalled(AppUrl2()));
  }

  // Re-enabling System Web Apps installs even without a version change.
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(7u, install_requests.size());
  EXPECT_FALSE(install_requests[5].force_reinstall);
  EXPECT_FALSE(install_requests[6].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Changing the install URL of a system app propagates even without a version
  // change.
  system_apps.find(SystemAppType::SETTINGS)->second.install_url = AppUrl3();
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(9u, install_requests.size());
  EXPECT_FALSE(install_requests[7].force_reinstall);
  EXPECT_FALSE(install_requests[8].force_reinstall);
  EXPECT_FALSE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
  EXPECT_TRUE(IsInstalled(AppUrl3()));
}

TEST_F(SystemWebAppManagerTest, UpdateOnLocaleChange) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager()->install_requests();

  system_web_app_manager()->SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
  system_web_app_manager()->SetSystemAppsForTesting(system_apps);

  // Simulate first execution.
  pending_app_manager()->SetInstallResultCode(
      InstallResultCode::kSuccessNewInstall);
  system_web_app_manager()->set_current_locale("en-US");
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Change locale setting, should trigger reinstall.
  pending_app_manager()->SetInstallResultCode(
      InstallResultCode::kSuccessNewInstall);
  system_web_app_manager()->set_current_locale("ja");
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, install_requests.size());
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Do not reinstall because locale is not changed.
  system_web_app_manager()->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3u, install_requests.size());
  EXPECT_FALSE(install_requests[2].force_reinstall);
}

TEST_F(SystemWebAppManagerTest, InstallResultHistogram) {
  base::HistogramTester histograms;
  const std::string settings_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kSettingsAppNameForLogging;
  const std::string discover_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kDiscoverAppNameForLogging;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";
  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
    system_web_app_manager()->SetSystemAppsForTesting(system_apps);

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 0);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 0);
    histograms.ExpectTotalCount(profile_install_result_histogram, 0);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 0);

    system_web_app_manager()->Start();
    base::RunLoop().RunUntilIdle();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 1);
    histograms.ExpectBucketCount(settings_app_install_result_histogram,
                                 InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(profile_install_result_histogram, 1);
    histograms.ExpectBucketCount(profile_install_result_histogram,
                                 InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }
  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
    system_apps.emplace(SystemAppType::DISCOVER,
                        SystemAppInfo(kDiscoverAppNameForLogging, AppUrl2()));

    system_web_app_manager()->SetSystemAppsForTesting(system_apps);
    pending_app_manager()->SetInstallResultCode(
        InstallResultCode::kWebAppDisabled);

    system_web_app_manager()->Start();
    base::RunLoop().RunUntilIdle();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kWebAppDisabled, 2);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 2);
    histograms.ExpectBucketCount(settings_app_install_result_histogram,
                                 InstallResultCode::kWebAppDisabled, 1);
    histograms.ExpectBucketCount(discover_app_install_result_histogram,
                                 InstallResultCode::kWebAppDisabled, 1);
  }
  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppNameForLogging, AppUrl1()));
    system_web_app_manager()->SetSystemAppsForTesting(system_apps);
    pending_app_manager()->SetInstallResultCode(
        InstallResultCode::kWebAppDisabled);

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 2);
    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);

    system_web_app_manager()->Start();
    system_web_app_manager()->Shutdown();
    base::RunLoop().RunUntilIdle();

    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kWebAppDisabled, 2);

    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    // If install was interrupted by shutdown, do not report duration.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 2);
  }
}

}  // namespace web_app
