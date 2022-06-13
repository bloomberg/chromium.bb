// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/cloud_reporting_policy_handler.h"

#include "base/values.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

#if !defined(OS_CHROMEOS)
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif  //! defined(OS_CHROMEOS)

namespace enterprise_reporting {

CloudReportingPolicyHandler::CloudReportingPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kCloudReportingEnabled,
                                        base::Value::Type::BOOLEAN) {}

CloudReportingPolicyHandler::~CloudReportingPolicyHandler() = default;

bool CloudReportingPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!policies.GetValue(policy_name()))
    return true;
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;
#if !defined(OS_CHROMEOS)
  // We don't return false here because machine enrollment status change may
  // not trigger this PolicyHandler later.
  if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
    errors->AddError(policy_name(),
                     IDS_POLICY_CLOUD_MANAGEMENT_ENROLLMENT_ONLY_ERROR);
#endif  //! defined(OS_CHROMEOS)
  return true;
}

void CloudReportingPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* cloud_reporting_policy_value =
      policies.GetValue(policy_name());

  if (cloud_reporting_policy_value) {
    prefs->SetBoolean(kCloudReportingEnabled,
                      cloud_reporting_policy_value->GetBool());
  }
}

}  // namespace enterprise_reporting
