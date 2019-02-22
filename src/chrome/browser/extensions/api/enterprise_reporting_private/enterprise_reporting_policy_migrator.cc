// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_policy_migrator.h"

#include "components/policy/policy_constants.h"

namespace extensions {
namespace enterprise_reporting {

namespace {

// Hash of the Extension ID for the Chrome Reporting Extension.
// https://chrome.google.com/webstore/detail/chrome-reporting-extensio/emahakmocgideepebncgnmlmliepgpgb
const char kStableHashedExtensionId[] =
    "FD15C63ABA854733FDCBC1D4D34A71E963A12ABD";

// Hash of the beta extension ID.
const char kBetaHashedExtensionId[] =
    "08455FA7CB8734168378F731B00B354CEEE0088F";

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
  CopyPoliciesIfUnset(bundle, kStableHashedExtensionId, kMigrations);
  CopyPoliciesIfUnset(bundle, kBetaHashedExtensionId, kMigrations);
}

}  // namespace enterprise_reporting
}  // namespace extensions
