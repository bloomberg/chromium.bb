// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/incognito_mode_policy_handler.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

// Tests Incognito mode availability preference setting.
class IncognitoModePolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new IncognitoModePolicyHandler));
  }
 protected:
  static const int kIncognitoModeAvailabilityNotSet = -1;

  enum ObsoleteIncognitoEnabledValue {
    INCOGNITO_ENABLED_UNKNOWN,
    INCOGNITO_ENABLED_TRUE,
    INCOGNITO_ENABLED_FALSE
  };

  void SetPolicies(ObsoleteIncognitoEnabledValue incognito_enabled,
                   int availability) {
    PolicyMap policy;
    if (incognito_enabled != INCOGNITO_ENABLED_UNKNOWN) {
      policy.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(incognito_enabled ==
                                               INCOGNITO_ENABLED_TRUE),
                 nullptr);
    }
    if (availability >= 0) {
      policy.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(availability), nullptr);
    }
    UpdateProviderPolicy(policy);
  }

  void VerifyValues(IncognitoModePrefs::Availability availability) {
    const base::Value* value = NULL;
    EXPECT_TRUE(store_->GetValue(prefs::kIncognitoModeAvailability, &value));
    EXPECT_TRUE(base::Value(availability).Equals(value));
  }
};

// The following testcases verify that if the obsolete IncognitoEnabled
// policy is not set, the IncognitoModeAvailability values should be copied
// from IncognitoModeAvailability policy to pref "as is".
TEST_F(IncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoEnabled) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::ENABLED);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoDisabled) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::DISABLED);
  VerifyValues(IncognitoModePrefs::DISABLED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndIncognitoForced) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, IncognitoModePrefs::FORCED);
  VerifyValues(IncognitoModePrefs::FORCED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       NoObsoletePolicyAndNoIncognitoAvailability) {
  SetPolicies(INCOGNITO_ENABLED_UNKNOWN, kIncognitoModeAvailabilityNotSet);
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kIncognitoModeAvailability, &value));
}

// Checks that if the obsolete IncognitoEnabled policy is set, if sets
// the IncognitoModeAvailability preference only in case
// the IncognitoModeAvailability policy is not specified.
TEST_F(IncognitoModePolicyHandlerTest,
       ObsoletePolicyDoesNotAffectAvailabilityEnabled) {
  SetPolicies(INCOGNITO_ENABLED_FALSE, IncognitoModePrefs::ENABLED);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       ObsoletePolicyDoesNotAffectAvailabilityForced) {
  SetPolicies(INCOGNITO_ENABLED_TRUE, IncognitoModePrefs::FORCED);
  VerifyValues(IncognitoModePrefs::FORCED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       ObsoletePolicySetsPreferenceToEnabled) {
  SetPolicies(INCOGNITO_ENABLED_TRUE, kIncognitoModeAvailabilityNotSet);
  VerifyValues(IncognitoModePrefs::ENABLED);
}

TEST_F(IncognitoModePolicyHandlerTest,
       ObsoletePolicySetsPreferenceToDisabled) {
  SetPolicies(INCOGNITO_ENABLED_FALSE, kIncognitoModeAvailabilityNotSet);
  VerifyValues(IncognitoModePrefs::DISABLED);
}

}  // namespace policy
