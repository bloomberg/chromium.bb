// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"
#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

SafeBrowsingNavigationThrottle::SafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle,
    SafeBrowsingUIManager* ui_manager)
    : content::NavigationThrottle(handle), ui_manager_(ui_manager) {}

const char* SafeBrowsingNavigationThrottle::GetNameForLogging() {
  return "SafeBrowsingNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SafeBrowsingNavigationThrottle::WillFailRequest() {
  if (ui_manager_) {
    security_interstitials::UnsafeResource resource;
    content::NavigationHandle* handle = navigation_handle();
    if (ui_manager_->PopUnsafeResourceForURL(handle->GetURL(), &resource)) {
      SafeBrowsingBlockingPage* blocking_page =
          SafeBrowsingBlockingPage::CreateBlockingPage(
              ui_manager_, handle->GetWebContents(), handle->GetURL(),
              resource);
      std::string error_page_content = blocking_page->GetHTMLContents();
      security_interstitials::SecurityInterstitialTabHelper::
          AssociateBlockingPage(handle->GetWebContents(),
                                handle->GetNavigationId(),
                                base::WrapUnique(blocking_page));
      return content::NavigationThrottle::ThrottleCheckResult(
          CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
    }
  }
  return content::NavigationThrottle::PROCEED;
}

}  // namespace weblayer
