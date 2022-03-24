// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace chromeos {

namespace {

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
const char kBogusId[] = "bogus";

scoped_refptr<const extensions::Extension> CreateExtensionFromValues(
    const std::string& id,
    ManifestLocation location,
    base::DictionaryValue* values,
    int flags) {
  values->SetString(extensions::manifest_keys::kName, "test");
  values->SetString(extensions::manifest_keys::kVersion, "0.1");
  values->SetInteger(extensions::manifest_keys::kManifestVersion, 2);
  std::string error;
  return extensions::Extension::Create(base::FilePath(),
                                       location,
                                       *values,
                                       flags,
                                       id,
                                       &error);
}

scoped_refptr<const extensions::Extension> CreateRegularExtension(
    const std::string& id) {
  base::DictionaryValue values;
  return CreateExtensionFromValues(id, ManifestLocation::kInternal, &values,
                                   extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateExternalComponentExtension() {
  base::DictionaryValue values;
  return CreateExtensionFromValues(std::string(),
                                   ManifestLocation::kExternalComponent,
                                   &values, extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateComponentExtension() {
  base::DictionaryValue values;
  return CreateExtensionFromValues(std::string(), ManifestLocation::kComponent,
                                   &values, extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreateHostedApp() {
  base::DictionaryValue values;
  values.SetKey(extensions::manifest_keys::kApp, base::DictionaryValue());
  values.SetPath(extensions::manifest_keys::kWebURLs, base::ListValue());
  return CreateExtensionFromValues(std::string(), ManifestLocation::kInternal,
                                   &values, extensions::Extension::NO_FLAGS);
}

scoped_refptr<const extensions::Extension> CreatePlatformAppWithExtraValues(
    const base::DictionaryValue* extra_values,
    ManifestLocation location,
    int flags) {
  base::DictionaryValue values;
  values.SetString("app.background.page", "background.html");
  values.MergeDictionary(extra_values);
  return CreateExtensionFromValues(std::string(), location, &values, flags);
}

scoped_refptr<const extensions::Extension> CreatePlatformApp() {
  base::DictionaryValue values;
  return CreatePlatformAppWithExtraValues(&values, ManifestLocation::kInternal,
                                          extensions::Extension::NO_FLAGS);
}

}  // namespace

TEST(DeviceLocalAccountManagementPolicyProviderTest, PublicSession) {
  DeviceLocalAccountManagementPolicyProvider
      provider(policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  // Set the login state to a public session.
  ScopedTestPublicSessionLoginState login_state;

  // Verify that if an extension's location has been whitelisted for use in
  // public sessions, the extension can be installed.
  scoped_refptr<const extensions::Extension> extension =
      CreateExternalComponentExtension();
  ASSERT_TRUE(extension.get());
  std::u16string error;
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  extension = CreateComponentExtension();
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  // Verify that if an extension's type has been whitelisted for use in
  // device-local accounts, the extension can be installed.
  extension = CreateHostedApp();
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  // Verify that if an extension's ID has been explicitly whitelisted for use in
  // device-local accounts, the extension can be installed.
  extension = CreateRegularExtension(kWhitelistedId);
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_EQ(std::u16string(), error);
  error.clear();

  // Verify that if neither the location, type nor the ID of an extension have
  // been whitelisted for use in public sessions, the extension cannot be
  // installed.
  extension = CreateRegularExtension(std::string());
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
  EXPECT_NE(std::u16string(), error);
  error.clear();

  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a minimal platform app can be installed from location
  // EXTERNAL_POLICY_DOWNLOAD.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicyDownload,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a minimal platform app cannot be installed from location
  // UNPACKED.
  {
    base::DictionaryValue values;
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kUnpacked, extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with all safe manifest entries can be installed.
  {
    base::DictionaryValue values;
    values.SetString(extensions::manifest_keys::kDescription, "something");
    values.SetString(extensions::manifest_keys::kShortName, "something else");
    base::ListValue permissions;
    permissions.Append("alarms");
    permissions.Append("background");
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));
    base::ListValue optional_permissions;
    optional_permissions.Append("alarms");
    optional_permissions.Append("background");
    values.SetKey(extensions::manifest_keys::kOptionalPermissions,
                  std::move(optional_permissions));
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unknown manifest entry cannot be
  // installed.
  {
    base::DictionaryValue values;
    values.SetString("not_whitelisted", "something");
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unsafe manifest entry cannot be
  // installed.  Since the program logic is based entirely on whitelists, there
  // is no significant advantage in testing all unsafe manifest entries
  // individually.
  {
    base::DictionaryValue values;
    values.SetPath("chrome_settings_overrides", base::DictionaryValue());
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unknown manifest entry under "app"
  // cannot be installed.
  {
    base::DictionaryValue values;
    values.SetString("app.not_whitelisted2", "something2");
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with a safe manifest entry under "app" can be
  // installed.
  {
    base::DictionaryValue values;
    values.SetString("app.content_security_policy", "something2");
    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a hosted app with a safe manifest entry under "app" can be
  // installed.
  {
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kApp, base::DictionaryValue());
    values.SetPath(extensions::manifest_keys::kWebURLs, base::ListValue());
    values.SetString("app.content_security_policy", "something2");
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a theme with a safe manifest entry under "app" cannot be
  // installed.
  {
    base::DictionaryValue values;
    values.SetKey("theme", base::DictionaryValue());
    values.SetString("app.content_security_policy", "something2");
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unknown permission entry cannot be
  // installed.
  {
    base::ListValue permissions;
    permissions.Append("not_whitelisted_permission");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unsafe permission entry cannot be
  // installed.  Since the program logic is based entirely on whitelists, there
  // is no significant advantage in testing all unsafe permissions individually.
  {
    base::ListValue permissions;
    permissions.Append("experimental");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an unsafe optional permission entry cannot
  // be installed.
  {
    base::ListValue permissions;
    permissions.Append("experimental");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kOptionalPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with an url_handlers manifest entry and which is
  // not installed through the web store cannot be installed.
  {
    base::ListValue matches;
    matches.Append("https://example.com/*");
    base::DictionaryValue values;
    values.SetPath("url_handlers.example_com.matches", std::move(matches));
    values.SetString("url_handlers.example_com.title", "example title");

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with a url_handlers manifest entry and which is
  // installed through the web store can be installed.
  {
    base::ListValue matches;
    matches.Append("https://example.com/*");
    base::DictionaryValue values;
    values.SetPath("url_handlers.example_com.matches", std::move(matches));
    values.SetString("url_handlers.example_com.title", "example title");

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::FROM_WEBSTORE);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with remote URL permissions can be installed.
  {
    base::ListValue permissions;
    permissions.Append("https://example.com/");
    permissions.Append("http://example.com/");
    permissions.Append("ftp://example.com/");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that an extension with remote URL permissions cannot be installed.
  {
    base::ListValue permissions;
    permissions.Append("https://example.com/");
    permissions.Append("http://example.com/");
    permissions.Append("ftp://example.com/");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with a local URL permission cannot be installed.
  {
    base::ListValue permissions;
    permissions.Append("file:///some/where");
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with socket dictionary permission can be
  // installed.
  {
    auto socket = std::make_unique<base::DictionaryValue>();
    base::ListValue tcp_list;
    tcp_list.Append("tcp-connect");
    socket->SetKey("socket", std::move(tcp_list));
    base::ListValue permissions;
    permissions.Append(std::move(socket));
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a platform app with unknown dictionary permission cannot be
  // installed.
  {
    auto socket = std::make_unique<base::DictionaryValue>();
    base::ListValue tcp_list;
    tcp_list.Append("unknown_value");
    socket->SetKey("unknown_key", std::move(tcp_list));
    base::ListValue permissions;
    permissions.Append(std::move(socket));
    base::DictionaryValue values;
    values.SetKey(extensions::manifest_keys::kPermissions,
                  std::move(permissions));

    extension = CreatePlatformAppWithExtraValues(
        &values, ManifestLocation::kExternalPolicy,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }

  // Verify that an extension can be installed.
  {
    base::DictionaryValue values;
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a shared_module can be installed.
  {
    base::DictionaryValue values;
    values.SetPath("export.whitelist", base::ListValue());
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a theme can be installed.
  {
    base::DictionaryValue values;
    values.SetKey("theme", base::DictionaryValue());
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }

  // Verify that a legacy_packaged_app cannot be installed and that it cannot
  // have an "app" manifest entry.
  {
    base::DictionaryValue values;
    values.SetString("app.launch.local_path", "something");
    extension = CreateExtensionFromValues(
        std::string(), ManifestLocation::kExternalPolicy, &values,
        extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(extension);

    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();
  }
}

TEST(DeviceLocalAccountManagementPolicyProviderTest, KioskAppSessions) {
  std::vector<policy::DeviceLocalAccount::Type> types = {
      policy::DeviceLocalAccount::TYPE_KIOSK_APP,
      policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP};

  for (auto type : types) {
    LOG(INFO) << "Testing device local account type = "<< static_cast<int>(type);
    DeviceLocalAccountManagementPolicyProvider provider(type);

    // Verify that a platform app can be installed.
    scoped_refptr<const extensions::Extension> extension = CreatePlatformApp();
    ASSERT_TRUE(extension.get());
    std::u16string error;
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();

    // Verify that an extension whose location has been whitelisted for use in
    // other types of device-local accounts cannot be installed in a single-app
    // kiosk session.
    extension = CreateExternalComponentExtension();
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();

    extension = CreateComponentExtension();
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();

    // Verify that an extension whose type has been whitelisted for use in other
    // types of device-local accounts cannot be installed in a single-app kiosk
    // session.
    extension = CreateHostedApp();
    ASSERT_TRUE(extension.get());
    EXPECT_FALSE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_NE(std::u16string(), error);
    error.clear();

    // Verify that an extension whose ID has been whitelisted for use in other
    // types of device-local accounts cannot be installed in a single-app kiosk
    // session.
    extension = CreateRegularExtension(kWhitelistedId);
    ASSERT_TRUE(extension.get());
    EXPECT_TRUE(provider.UserMayLoad(extension.get(), &error));
    EXPECT_EQ(std::u16string(), error);
    error.clear();
  }
}

TEST(DeviceLocalAccountManagementPolicyProviderTest, IsWhitelisted) {
  // Whitelisted extension, Chrome Remote Desktop.
  EXPECT_TRUE(DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
      kWhitelistedId));

  // Bogus extension ID (never true).
  EXPECT_FALSE(DeviceLocalAccountManagementPolicyProvider::IsWhitelisted(
      kBogusId));
}

}  // namespace chromeos
