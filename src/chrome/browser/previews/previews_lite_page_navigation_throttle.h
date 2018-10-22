// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
struct OpenURLParams;
}

// This class does the actual decision making about when to serve a Lite Page
// Server Preview, and the legwork to trigger the Preview navigation. When a
// Preview is triggered, it will cancel the incoming navigation and PostTask a
// new one to the Previews Server.
class PreviewsLitePageNavigationThrottle : public content::NavigationThrottle {
 public:
  PreviewsLitePageNavigationThrottle(
      content::NavigationHandle* handle,
      PreviewsLitePageNavigationThrottleManager* manager);

  ~PreviewsLitePageNavigationThrottle() override;

  // Attempts to extract the original URL from the given Previews URL. Returns
  // false if |url| is not a valid Preview URL. It is ok to pass nullptr for
  // |original_url| if you only want the boolean return value.
  static bool GetOriginalURL(GURL url, std::string* original_url);

 private:
  FRIEND_TEST_ALL_PREFIXES(PreviewsLitePageNavigationThrottleTest,
                           TestGetPreviewsURL);

  // The current effective connection type;
  net::EffectiveConnectionType GetECT() const;

  // Returns true if the current URL is eligible for the preview. Does not check
  // any blacklisting or single bypass.
  bool IsEligibleForPreview() const;

  // Returns the URL for a preview of the current URL given by the navigation
  // handle.
  GURL GetPreviewsURL() const;

  // This method is called on every request and redirect. It checks all
  // eligibility and blacklisting criteria for the current URL and will return
  // CANCEL when the preview is triggered and post a new navigation, or will
  // return PROCEED if the preview is not triggered.
  content::NavigationThrottle::ThrottleCheckResult MaybeNavigateToPreview()
      const;

  // Can be called by any of the content::NavigationThrottle implementation
  // methods to create a new navigation with the give |content::OpenURLParams|.
  // Returns the |content::NavigationThrottle::ThrottleCheckResult| for the
  // implemented method to return.
  content::NavigationThrottle::ThrottleCheckResult CreateNewNavigation(
      content::OpenURLParams url_params) const;

  // Can be called by any of the content::NavigationThrottle implementation
  // methods to trigger the preview. Returns the
  // |content::NavigationThrottle::ThrottleCheckResult| for the implemented
  // method to return.
  content::NavigationThrottle::ThrottleCheckResult TriggerPreview() const;

  // content::NavigationThrottle implementation:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  // The manager class to report state changes to and get state from. Outlives
  // |this|.
  PreviewsLitePageNavigationThrottleManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsLitePageNavigationThrottle);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_LITE_PAGE_NAVIGATION_THROTTLE_H_
