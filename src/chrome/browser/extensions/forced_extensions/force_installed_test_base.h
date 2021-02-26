// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TEST_BASE_H_

#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

namespace extensions {

class ExtensionRegistry;
class InstallStageTracker;
class ForceInstalledTracker;

// This class is extended by tests to provide a setup for tracking installation
// of force extensions. It also provides helper functions for creating and
// setting ExtensionInstallForcelist policy value.
class ForceInstalledTestBase : public testing::Test {
 public:
  ForceInstalledTestBase();
  ~ForceInstalledTestBase() override;

  ForceInstalledTestBase(const ForceInstalledTestBase&) = delete;
  ForceInstalledTestBase& operator=(const ForceInstalledTestBase&) = delete;

 protected:
  void SetUp() override;

  // Creates and sets value for ExtensionInstallForcelist policy and
  // kInstallForceList preference. |is_from_store| tells whether the extensions
  // specified in the policy should have an update URL from CWS or not.
  void SetupForceList(bool is_from_store);

  // Creates and sets empty value for ExtensionInstallForcelist policy and
  // kInstallForceList preference.
  void SetupEmptyForceList();

  Profile* profile() const { return profile_; }

  sync_preferences::TestingPrefServiceSyncable* prefs() const { return prefs_; }

  ExtensionRegistry* registry() const { return registry_; }

  InstallStageTracker* install_stage_tracker() const {
    return install_stage_tracker_;
  }

  ForceInstalledTracker* force_installed_tracker() const {
    return force_installed_tracker_.get();
  }

  static const char kExtensionId1[];
  static const char kExtensionId2[];
  static const char kExtensionName1[];
  static const char kExtensionName2[];
  static const char kExtensionUpdateUrl[];
  static const char kOffStoreUpdateUrl[];

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  ExtensionRegistry* registry_;
  InstallStageTracker* install_stage_tracker_;
  std::unique_ptr<ForceInstalledTracker> force_installed_tracker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_FORCE_INSTALLED_TEST_BASE_H_
