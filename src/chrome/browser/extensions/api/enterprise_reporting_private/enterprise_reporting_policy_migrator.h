// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_POLICY_MIGRATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_POLICY_MIGRATOR_H_

#include "chrome/browser/policy/chrome_extension_policy_migrator.h"

namespace extensions {
namespace enterprise_reporting {

class EnterpriseReportingPolicyMigrator
    : public policy::ChromeExtensionPolicyMigrator {
 public:
  EnterpriseReportingPolicyMigrator();
  ~EnterpriseReportingPolicyMigrator() override;

  void Migrate(policy::PolicyBundle* bundle) override;
};

}  // namespace enterprise_reporting
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_POLICY_MIGRATOR_H_
