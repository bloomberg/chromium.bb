// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/browser_signin_policy_handler.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/pref_names.h"

namespace policy {
BrowserSigninPolicyHandler::BrowserSigninPolicyHandler(Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kBrowserSignin,
          chrome_schema.GetKnownProperty(key::kBrowserSignin),
          SCHEMA_ALLOW_UNKNOWN) {}

BrowserSigninPolicyHandler::~BrowserSigninPolicyHandler() {}

bool BrowserSigninPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return true;

  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  if (!IsForcedBrowserSigninEnabled()) {
    absl::optional<int> optional_int_value = value->GetIfInt();
    if (optional_int_value) {
      const int int_value = optional_int_value.value();
      if (int_value == static_cast<int>(BrowserSigninMode::kForced)) {
        // Don't return false because in this case the policy falls back to
        // BrowserSigninMode::kEnabled
        errors->AddError(policy_name(), IDS_POLICY_LEVEL_ERROR);
      }
    }
  }

  return true;
}

void BrowserSigninPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  absl::optional<int> optional_int_value = value->GetIfInt();
  if (!optional_int_value)
    return;

  const int int_value = optional_int_value.value();
  if (static_cast<int>(BrowserSigninMode::kDisabled) > int_value ||
      static_cast<int>(BrowserSigninMode::kForced) < int_value) {
    SYSLOG(ERROR) << "Unexpected value for BrowserSigninMode: " << int_value;
    NOTREACHED();
    return;
  }

  switch (static_cast<BrowserSigninMode>(int_value)) {
    case BrowserSigninMode::kForced:
      if (IsForcedBrowserSigninEnabled()) {
        prefs->SetInteger(prefs::kBrowserSigninPolicy,
                          static_cast<int>(BrowserSigninMode::kForced));
        break;
      }
      FALLTHROUGH;
    case BrowserSigninMode::kEnabled:
      prefs->SetInteger(prefs::kBrowserSigninPolicy,
                        static_cast<int>(BrowserSigninMode::kEnabled));
      break;
    case BrowserSigninMode::kDisabled:
      prefs->SetInteger(prefs::kBrowserSigninPolicy,
                        static_cast<int>(BrowserSigninMode::kDisabled));
      break;
  }
}

}  // namespace policy
