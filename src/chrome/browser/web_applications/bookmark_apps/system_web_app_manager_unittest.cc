// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const char kWindowedUrl[] = "https://windowed.example";
const char kTabbedUrl[] = "https://tabbed.example";
const char kDefaultContainerUrl[] = "https://default-container.example";

PendingAppManager::AppInfo GetWindowedAppInfo() {
  return PendingAppManager::AppInfo(
      GURL(kWindowedUrl), LaunchContainer::kWindow,
      InstallSource::kSystemInstalled, false /* create_shortcuts */);
}

PendingAppManager::AppInfo GetTabbedAppInfo() {
  return PendingAppManager::AppInfo(GURL(kTabbedUrl), LaunchContainer::kTab,
                                    InstallSource::kSystemInstalled,
                                    false /* create_shortcuts */);
}

}  // namespace

class TestSystemWebAppManager : public SystemWebAppManager {
 public:
  TestSystemWebAppManager(Profile* profile,
                          PendingAppManager* pending_app_manager,
                          std::vector<PendingAppManager::AppInfo> system_apps)
      : SystemWebAppManager(profile, pending_app_manager),
        system_apps_(std::move(system_apps)) {}
  ~TestSystemWebAppManager() override {}

  std::vector<PendingAppManager::AppInfo> CreateSystemWebApps() override {
    return std::move(system_apps_);
  }

 private:
  std::vector<PendingAppManager::AppInfo> system_apps_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemWebAppManager);
};

class SystemWebAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  SystemWebAppManagerTest() = default;
  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeatures({features::kSystemWebApps}, {});

    // Reset WebAppProvider so that its SystemWebAppManager doesn't interfere
    // with tests.
    WebAppProvider::Get(profile())->Reset();
  }

  void SimulatePreviouslyInstalledApp(
      TestPendingAppManager* pending_app_manager,
      GURL url,
      InstallSource install_source) {
    std::string id =
        crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExtensionIdsMap extension_ids_map(profile()->GetPrefs());
    extension_ids_map.Insert(url, id, install_source);

    pending_app_manager->SimulatePreviouslyInstalledApp(url, install_source);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerTest);
};

// Test that System Apps are uninstalled with the feature disabled.
TEST_F(SystemWebAppManagerTest, Disabled) {
  base::test::ScopedFeatureList disable_feature_list;
  disable_feature_list.InitWithFeatures({}, {features::kSystemWebApps});

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  SimulatePreviouslyInstalledApp(pending_app_manager.get(), GURL(kWindowedUrl),
                                 InstallSource::kSystemInstalled);

  std::vector<PendingAppManager::AppInfo> system_apps;
  system_apps.push_back(GetWindowedAppInfo());

  TestSystemWebAppManager system_web_app_manager(
      profile(), pending_app_manager.get(), std::move(system_apps));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pending_app_manager->install_requests().empty());

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({GURL(kWindowedUrl)}),
            pending_app_manager->uninstall_requests());
}

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  std::vector<PendingAppManager::AppInfo> system_apps;
  system_apps.push_back(GetWindowedAppInfo());
  system_apps.push_back(GetTabbedAppInfo());

  TestSystemWebAppManager system_web_app_manager(
      profile(), pending_app_manager.get(), std::move(system_apps));
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();
  EXPECT_FALSE(apps_to_install.empty());
}

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(pending_app_manager.get(), GURL(kWindowedUrl),
                                 InstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(pending_app_manager.get(), GURL(kTabbedUrl),
                                 InstallSource::kSystemInstalled);
  SimulatePreviouslyInstalledApp(pending_app_manager.get(),
                                 GURL(kDefaultContainerUrl),
                                 InstallSource::kInternal);
  std::vector<PendingAppManager::AppInfo> system_apps;
  system_apps.push_back(GetWindowedAppInfo());

  TestSystemWebAppManager system_web_app_manager(
      profile(), pending_app_manager.get(), std::move(system_apps));
  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the System App list.
  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetWindowedAppInfo());
  EXPECT_EQ(pending_app_manager->install_requests(), expected_apps_to_install);

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            pending_app_manager->uninstall_requests());
}

}  // namespace web_app
