// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/sync/base/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // defined(OS_CHROMEOS)

namespace syncer {
namespace {

TEST(SyncPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(prefs.GetValue(prefs::kSyncManaged, nullptr));
}

TEST(SyncPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(false), nullptr);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Sync should not set the pref.
  EXPECT_FALSE(prefs.GetValue(prefs::kSyncManaged, nullptr));
}

TEST(SyncPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(true), nullptr);
  SyncPolicyHandler handler;
  PrefValueMap prefs;
  handler.ApplyPolicySettings(policy, &prefs);

  // Sync should be flagged as managed.
  const base::Value* value = nullptr;
  EXPECT_TRUE(prefs.GetValue(prefs::kSyncManaged, &value));
  ASSERT_TRUE(value);
  bool sync_managed = false;
  bool result = value->GetAsBoolean(&sync_managed);
  ASSERT_TRUE(result);
  EXPECT_TRUE(sync_managed);
}

TEST(SyncPolicyHandlerTest, SyncTypesListDisabled) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::kSyncBookmarks, true);
  prefs.SetBoolean(prefs::kSyncPreferences, true);
  prefs.SetBoolean(prefs::kSyncAutofill, true);
  prefs.SetBoolean(prefs::kSyncThemes, true);

  // Create a policy that disables some types.
  policy::PolicyMap policy;
  base::ListValue disabled_types;
  disabled_types.AppendString("bookmarks");
  disabled_types.AppendString("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, disabled_types.CreateDeepCopy(),
             nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Prefs in the policy should be disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncBookmarks, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncPreferences, &enabled));
  EXPECT_FALSE(enabled);

  // Prefs that are not part of the policy are still enabled.
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncAutofill, &enabled));
  EXPECT_TRUE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncThemes, &enabled));
  EXPECT_TRUE(enabled);
}

#if defined(OS_CHROMEOS)
class SyncPolicyHandlerOsTest : public testing::Test {
 public:
  SyncPolicyHandlerOsTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SyncPolicyHandlerOsTest, SyncTypesListDisabled_OsTypes) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::kSyncOsApps, true);
  prefs.SetBoolean(prefs::kSyncOsPreferences, true);
  prefs.SetBoolean(prefs::kSyncWifiConfigurations, true);

  // Create a policy that disables the types.
  policy::PolicyMap policy;
  base::ListValue disabled_types;
  disabled_types.AppendString("osApps");
  disabled_types.AppendString("osPreferences");
  disabled_types.AppendString("wifiConfigurations");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, disabled_types.CreateDeepCopy(),
             nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Prefs in the policy are disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncOsApps, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncOsPreferences, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncWifiConfigurations, &enabled));
  EXPECT_FALSE(enabled);
}

TEST_F(SyncPolicyHandlerOsTest, SyncTypesListDisabled_MigratedTypes) {
  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(prefs::kSyncOsApps, true);
  prefs.SetBoolean(prefs::kSyncOsPreferences, true);

  // Create a policy that disables the types, but using the original browser
  // policy names from before the SplitSettingsSync launch.
  policy::PolicyMap policy;
  base::ListValue disabled_types;
  disabled_types.AppendString("apps");
  disabled_types.AppendString("preferences");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, disabled_types.CreateDeepCopy(),
             nullptr);
  SyncPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // The equivalent OS types are disabled.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncOsApps, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(prefs::kSyncOsPreferences, &enabled));
  EXPECT_FALSE(enabled);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
}  // namespace syncer
