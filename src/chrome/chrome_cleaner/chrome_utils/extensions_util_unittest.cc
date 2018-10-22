// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extensions_util.h"

#include <vector>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/json_parser/test_json_parser.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace chrome_cleaner {
namespace {

const int kExtensionIdLength = 32;

struct TestRegistryEntry {
  HKEY hkey;
  const base::string16 path;
  const base::string16 name;
  const base::string16 value;
};

const TestRegistryEntry extension_forcelist_entries[] = {
    {HKEY_LOCAL_MACHINE, kChromePoliciesForcelistKeyPath, L"test1",
     L"ababababcdcdcdcdefefefefghghghgh;https://test.test/crx"
     L"update2/crx"},
    {HKEY_CURRENT_USER, kChromePoliciesForcelistKeyPath, L"test2",
     L"aaaabbbbccccddddeeeeffffgggghhhh;https://test.test/crx"
     L"update2/crx"}};

const wchar_t kFakeChromeFolder[] = L"google\\chrome\\application\\42.12.34.56";

const wchar_t kTestExtensionId1[] = L"ababababcdcdcdcdefefefefghghghgh";
const wchar_t kTestExtensionId2[] = L"aaaabbbbccccddddeeeeffffgggghhhh";
const char kDefaultExtensionsJson[] =
    R"(
    {
      "ababababcdcdcdcdefefefefghghghgh" : {
        "external_update_url":"https://test.test/crx"
      },
      "aaaabbbbccccddddeeeeffffgggghhhh" : {
        "external_update_url":"https://test.test/crx"
      },
      // Google Sheets
      "aapocclcgogkmnckokdopfmhonfmgoek" : {
        "external_update_url":"https://test.test/crx"
      },
    })";
const char kInvalidDefaultExtensionsJson[] = "{ json: invalid }";

// ExtensionSettings that has two force_installed extensions and two not.
const wchar_t kExtensionSettingsJson[] =
    LR"(
    {
      "ababababcdcdcdcdefefefefghghghgh": {
        "installation_mode": "force_installed",
        "update_url":"https://test.test/crx"
      },
      "aaaabbbbccccddddeeeeffffgggghhhh": {
        "installation_mode": "force_installed",
        "update_url":"https://test.test/crx"
      },
      "extensionwithinstallmodeblockeda": {
        "installation_mode": "blocked",
        "update_url":"https://test.test/crx"
      },
      "extensionwithnosettingsabcdefghi": {}
    })";
const TestRegistryEntry extension_settings_entry = {
    HKEY_LOCAL_MACHINE, L"software\\policies\\google\\chrome",
    L"ExtensionSettings", kExtensionSettingsJson};

const wchar_t kChromeExePath[] = L"google\\chrome\\application";
const wchar_t kMasterPreferencesFileName[] = L"master_preferences";
const char kMasterPreferencesJson[] =
    R"(
    {
      "homepage": "http://dev.chromium.org/",
      "extensions": {
        "settings": {
          "ababababcdcdcdcdefefefefghghghgh": {
            "location": 1,
            "manifest": {
              "name": "Test extension"
            }
          },
          "aaaabbbbccccddddeeeeffffgggghhhh": {
            "location": 1,
            "manifest": {
              "name": "Another one"
            }
          }
        }
      }
    })";
const char kMasterPreferencesJsonNoExtensions[] =
    R"(
    {
      "homepage": "http://dev.chromium.org/"
    })";

bool ExtensionPolicyRegistryEntryFound(
    TestRegistryEntry test_entry,
    const std::vector<ExtensionPolicyRegistryEntry>& found_policies) {
  for (const ExtensionPolicyRegistryEntry& policy : found_policies) {
    base::string16 test_entry_value(test_entry.value);
    if (policy.extension_id == test_entry_value.substr(0, kExtensionIdLength) &&
        policy.hkey == test_entry.hkey && policy.path == test_entry.path &&
        policy.name == test_entry.name) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(ExtensionsUtilTest, GetExtensionForcelistRegistryPolicies) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  for (const TestRegistryEntry& policy : extension_forcelist_entries) {
    base::win::RegKey policy_key;
    ASSERT_EQ(ERROR_SUCCESS, policy_key.Create(policy.hkey, policy.path.c_str(),
                                               KEY_ALL_ACCESS));
    DCHECK(policy_key.Valid());
    ASSERT_EQ(ERROR_SUCCESS,
              policy_key.WriteValue(policy.name.c_str(), policy.value.c_str()));
  }

  std::vector<ExtensionPolicyRegistryEntry> policies;
  GetExtensionForcelistRegistryPolicies(&policies);

  for (const TestRegistryEntry& expected_result : extension_forcelist_entries) {
    EXPECT_TRUE(ExtensionPolicyRegistryEntryFound(expected_result, policies));
  }
}

TEST(ExtensionsUtilTest, GetNonWhitelistedDefaultExtensions) {
  // Set up a fake default extensions JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath fake_apps_dir(
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps"));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  base::FilePath default_extensions_file =
      fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kDefaultExtensionsJson,
                        sizeof(kDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  // Set up an invalid default extensions JSON file
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILESX86, &program_files_dir));

  fake_apps_dir =
      program_files_dir.Append(kFakeChromeFolder).Append(L"default_apps");
  ASSERT_TRUE(base::CreateDirectoryAndGetError(fake_apps_dir, nullptr));

  default_extensions_file = fake_apps_dir.Append(L"external_extensions.json");
  CreateFileWithContent(default_extensions_file, kInvalidDefaultExtensionsJson,
                        sizeof(kInvalidDefaultExtensionsJson) - 1);
  ASSERT_TRUE(base::PathExists(default_extensions_file));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  const base::string16 expected_extension_ids[] = {kTestExtensionId1,
                                                   kTestExtensionId2};
  ASSERT_EQ(base::size(expected_extension_ids), policies.size());
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));
}

TEST(ExtensionsUtilTest, GetNonWhitelistedDefaultExtensionsNoFilesFound) {
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::ScopedPathOverride program_files_x86_override(
      base::DIR_PROGRAM_FILESX86);

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetNonWhitelistedDefaultExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

TEST(ExtensionsUtilTest, GetExtensionSettingsForceInstalledExtensions) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey settings_key;
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.Create(extension_settings_entry.hkey,
                                extension_settings_entry.path.c_str(),
                                KEY_ALL_ACCESS));
  DCHECK(settings_key.Valid());
  ASSERT_EQ(ERROR_SUCCESS,
            settings_key.WriteValue(extension_settings_entry.name.c_str(),
                                    extension_settings_entry.value.c_str()));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  // Check that only the two force installed extensions were found
  const base::string16 expected_extension_ids[] = {kTestExtensionId1,
                                                   kTestExtensionId2};
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));

  // Also check that the collected registry entries match the values in the
  // registry.
  for (const ExtensionPolicyRegistryEntry& policy : policies) {
    EXPECT_EQ(policy.hkey, extension_settings_entry.hkey);
    EXPECT_EQ(policy.path, extension_settings_entry.path);
    EXPECT_EQ(policy.name, extension_settings_entry.name);
  }
}

TEST(ExtensionsUtilTest,
     GetExtensionSettingsForceInstalledExtensionsNoneFound) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyRegistryEntry> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetExtensionSettingsForceInstalledExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

TEST(ExtensionsUtilTest, GetMasterPreferencesExtensions) {
  // Set up a fake master preferences JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJson,
                        sizeof(kMasterPreferencesJson) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  const base::string16 expected_extension_ids[] = {kTestExtensionId1,
                                                   kTestExtensionId2};
  ASSERT_EQ(base::size(expected_extension_ids), policies.size());
  const base::string16 found_extension_ids[] = {policies[0].extension_id,
                                                policies[1].extension_id};
  EXPECT_THAT(expected_extension_ids,
              ::testing::UnorderedElementsAreArray(found_extension_ids));
}

TEST(ExtensionsUtilTest, GetMasterPreferencesExtensionsNoneFound) {
  // Set up a fake master preferences JSON file.
  base::ScopedPathOverride program_files_override(base::DIR_PROGRAM_FILES);
  base::FilePath program_files_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_PROGRAM_FILES, &program_files_dir));

  base::FilePath chrome_dir(program_files_dir.Append(kChromeExePath));
  ASSERT_TRUE(base::CreateDirectoryAndGetError(chrome_dir, nullptr));

  base::FilePath master_preferences =
      chrome_dir.Append(kMasterPreferencesFileName);
  CreateFileWithContent(master_preferences, kMasterPreferencesJsonNoExtensions,
                        sizeof(kMasterPreferencesJsonNoExtensions) - 1);
  ASSERT_TRUE(base::PathExists(master_preferences));

  TestJsonParser json_parser;
  std::vector<ExtensionPolicyFile> policies;
  base::WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED);
  GetMasterPreferencesExtensions(&json_parser, &policies, &done);
  ASSERT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));

  size_t expected_policies_found = 0;
  ASSERT_EQ(expected_policies_found, policies.size());
}

}  // namespace chrome_cleaner
