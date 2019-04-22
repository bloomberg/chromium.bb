// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace safe_browsing {

// This throttle monitors failed requests, and if a request failed due to it
// being blocked by Safe Browsing, it creates and displays an interstitial.
// This throttle is only created when Safe Browsing committed interstitials are
// enabled.
class SafeBrowsingNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit SafeBrowsingNavigationThrottle(content::NavigationHandle* handle);
  ~SafeBrowsingNavigationThrottle() override {}
  const char* GetNameForLogging() override;

  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_THROTTLE_H_
