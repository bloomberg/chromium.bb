// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_policy_migrator.h"

#include "components/policy/policy_constants.h"

namespace extensions {
namespace enterprise_reporting {

namespace {

// Extension ID for the Chrome Reporting Extension.
// https://chrome.google.com/webstore/detail/chrome-reporting-extensio/emahakmocgideepebncgnmlmliepgpgb
const char kStableExtensionId[] = "emahakmocgideepebncgnmlmliepgpgb";

// Beta extension ID.
const char kBetaExtensionId[] = "kigjhoekjcpdfjpimbdjegmgecmlicaf";

const policy::ExtensionPolicyMigrator::Migration kMigrations[] = {
    {"report_version_data", policy::key::kReportVersionData},
    {"report_policy_data", policy::key::kReportPolicyData},
    {"report_machine_id_data", policy::key::kReportMachineIDData},
    {"report_user_id_data", policy::key::kReportUserIDData},
};

}  // namespace

EnterpriseReportingPolicyMigrator::EnterpriseReportingPolicyMigrator() {}

EnterpriseReportingPolicyMigrator::~EnterpriseReportingPolicyMigrator() {}

void EnterpriseReportingPolicyMigrator::Migrate(policy::PolicyBundle* bundle) {
  CopyPoliciesIfUnset(bundle, kStableExtensionId, kMigrations);
  CopyPoliciesIfUnset(bundle, kBetaExtensionId, kMigrations);
}

}  // namespace enterprise_reporting
}  // namespace extensions
