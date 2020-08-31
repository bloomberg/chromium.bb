// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_

#include "chrome/browser/chromeos/policy/value_validation/onc_policy_value_validator_base.h"

namespace enterprise_management {
class CloudPolicySettings;
}

namespace policy {

class ONCUserPolicyValueValidator
    : public ONCPolicyValueValidatorBase<
          enterprise_management::CloudPolicySettings> {
 public:
  ONCUserPolicyValueValidator();

 protected:
  // ONCPolicyValueValidatorBase:
  base::Optional<std::string> GetONCStringFromPayload(
      const enterprise_management::CloudPolicySettings& policy_payload)
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ONCUserPolicyValueValidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_VALUE_VALIDATION_ONC_USER_POLICY_VALUE_VALIDATOR_H_
