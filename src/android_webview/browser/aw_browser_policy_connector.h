// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_POLICY_CONNECTOR_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_POLICY_CONNECTOR_H_

#include "base/macros.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"

namespace android_webview {

// Sets up and keeps the browser-global policy objects such as the PolicyService
// and the platform-specific PolicyProvider.
class AwBrowserPolicyConnector : public policy::BrowserPolicyConnectorBase {
 public:
  AwBrowserPolicyConnector();
  ~AwBrowserPolicyConnector() override;

 protected:
  // policy::BrowserPolicyConnectorBase:
  std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
  CreatePolicyProviders() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AwBrowserPolicyConnector);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_POLICY_CONNECTOR_H_
