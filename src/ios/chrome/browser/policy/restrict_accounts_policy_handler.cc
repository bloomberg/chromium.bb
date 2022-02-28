// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/restrict_accounts_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/pattern_account_restriction.h"

namespace policy {

RestrictAccountsPolicyHandler::RestrictAccountsPolicyHandler(
    Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kRestrictAccountsToPatterns,
          chrome_schema.GetKnownProperty(key::kRestrictAccountsToPatterns),
          SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

RestrictAccountsPolicyHandler::~RestrictAccountsPolicyHandler() {}

bool RestrictAccountsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return true;
  if (!ArePatternsValid(value)) {
    errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
  }
  if (!value->is_list())
    return false;
  // Even if the pattern is not valid, the policy should be applied.
  return true;
}

void RestrictAccountsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (value && value->is_list())
    prefs->SetValue(prefs::kRestrictAccountsToPatterns, value->Clone());
}

}  // namespace policy
