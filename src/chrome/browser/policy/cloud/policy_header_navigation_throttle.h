// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_NAVIGATION_THROTTLE_H_

#include <memory>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace policy {

class PolicyHeaderNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit PolicyHeaderNavigationThrottle(content::NavigationHandle*);
  ~PolicyHeaderNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_HEADER_NAVIGATION_THROTTLE_H_
