// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"

namespace extensions {
namespace {
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionSettingsWithUpdateUrlBlocking[] = R"({
  "update_url:https://clients2.google.com/service/update2/crx": {
    "installation_mode": "blocked"
  }
})";

constexpr char kExtensionSettingsWithWildcardBlocking[] = R"({
  "*": {
    "installation_mode": "blocked"
  }
})";

constexpr char kExtensionSettingsWithIdBlocked[] = R"({
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "blocked"
  }
})";

constexpr char kExtensionSettingsWithIdAllowed[] = R"({
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "allowed"
  }
})";

constexpr char kExtensionSettingsWithIdForced[] = R"({
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode": "force_installed",
    "update_url":"https://clients2.google.com/service/update2/crx"
  }
})";

}  // namespace

class ExtensionInstallStatusTest : public BrowserWithTestWindowTest {
 public:
  ExtensionInstallStatusTest() = default;

  std::string GenerateArgs(const char* id) {
    return base::StringPrintf(R"(["%s"])", id);
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder("extension").SetID(id).Build();
  }

  void SetExtensionSettings(const std::string& settings_string) {
    base::Optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings);
    SetPolicy(pref_names::kExtensionManagement,
              base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  void SetPolicy(const std::string& pref_name,
                 std::unique_ptr<base::Value> value) {
    profile()->GetTestingPrefService()->SetManagedPref(pref_name,
                                                       std::move(value));
  }

  void SetPendingList(const std::vector<ExtensionId>& ids) {
    std::unique_ptr<base::Value> id_values =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    for (const auto& id : ids) {
      base::Value request_data(base::Value::Type::DICTIONARY);
      request_data.SetKey(extension_misc::kExtensionRequestTimestamp,
                          ::util::TimeToValue(base::Time::Now()));
      id_values->SetKey(id, std::move(request_data));
    }
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCloudExtensionRequestIds, std::move(id_values));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallStatusTest);
};

TEST_F(ExtensionInstallStatusTest, ExtensionEnabled) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kEnabled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionDisabled) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kDisabled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionInstalledButDisabledByPolicy) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionTerminated) {
  ExtensionRegistry::Get(profile())->AddTerminated(
      CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kTerminated,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlacklisted) {
  ExtensionRegistry::Get(profile())->AddBlacklisted(
      CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kBlacklisted,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionAllowed) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionForceInstalledByPolicy) {
  SetExtensionSettings(kExtensionSettingsWithIdForced);
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  EXPECT_EQ(ExtensionInstallStatus::kForceInstalled,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByUpdateUrl) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithUpdateUrlBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedByWildcard) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockedById) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest,
       ExtensionBlockByUpdateUrlWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  SetExtensionSettings(kExtensionSettingsWithUpdateUrlBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockByWildcardWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kCanRequest,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, ExtensionBlockByIdWithRequestEnabled) {
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  // An extension that is blocked by its ID can't be requested anymore.
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsWaitingToBeReviewed) {
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetPendingList(ids);

  // The extension is blocked by wildcard and pending approval.
  SetExtensionSettings(kExtensionSettingsWithWildcardBlocking);
  EXPECT_EQ(ExtensionInstallStatus::kRequestPending,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsApproved) {
  // Extension is approved but not installed, returns as INSTALLABLE.
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetExtensionSettings(kExtensionSettingsWithIdAllowed);
  EXPECT_EQ(ExtensionInstallStatus::kInstallable,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

TEST_F(ExtensionInstallStatusTest, PendingExtenisonIsRejected) {
  // Extension is rejected, it should be moved from the pending list soon.
  SetPolicy(prefs::kCloudExtensionRequestEnabled,
            std::make_unique<base::Value>(true));
  std::vector<ExtensionId> ids = {kExtensionId};
  SetExtensionSettings(kExtensionSettingsWithIdBlocked);
  EXPECT_EQ(ExtensionInstallStatus::kBlockedByPolicy,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

// If an extension is disabled due to reason
// DISABLE_CUSTODIAN_APPROVAL_REQUIRED, then GetWebstoreExtensionInstallStatus()
// should return kCustodianApprovalRequired.
TEST_F(ExtensionInstallStatusTest, ExtensionCustodianApprovalRequired) {
  ExtensionPrefs::Get(profile())->AddDisableReason(
      kExtensionId,
      extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  EXPECT_EQ(ExtensionInstallStatus::kCustodianApprovalRequired,
            GetWebstoreExtensionInstallStatus(kExtensionId, profile()));
}

}  // namespace extensions
