// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORK_PREDICTION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_NETWORK_PREDICTION_POLICY_HANDLER_H_

#include "base/macros.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// Handles NetworkPrediction policies.
class NetworkPredictionPolicyHandler : public ConfigurationPolicyHandler {
 public:
  NetworkPredictionPolicyHandler();
  ~NetworkPredictionPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkPredictionPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORK_PREDICTION_POLICY_HANDLER_H_
