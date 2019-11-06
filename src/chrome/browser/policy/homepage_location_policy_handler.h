// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_HOMEPAGE_LOCATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_HOMEPAGE_LOCATION_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Handles the |kHomepageLocation| policy and sets the |kHomepage| pref. Makes
// sure that the URL is valid and uses a standard scheme (i.e. no Javascript
// etc.).
class HomepageLocationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  HomepageLocationPolicyHandler();
  ~HomepageLocationPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HomepageLocationPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_HOMEPAGE_LOCATION_POLICY_HANDLER_H_
