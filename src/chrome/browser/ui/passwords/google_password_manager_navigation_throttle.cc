// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/google_password_manager_navigation_throttle.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

using NavigationResult =
    GooglePasswordManagerNavigationThrottle::NavigationResult;

constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(2);

void NavigateToChromeSettingsPasswordsPage(
    content::NavigationHandle* navigation_handle) {
  navigation_handle->GetWebContents()->OpenURL(content::OpenURLParams(
      chrome::GetSettingsUrl(chrome::kPasswordManagerSubPage),
      navigation_handle->GetReferrer(), navigation_handle->GetFrameTreeNodeId(),
      WindowOpenDisposition::CURRENT_TAB,
      navigation_handle->GetPageTransition(),
      false /* is_renderer_initiated */));
}

void RecordNavigationResult(NavigationResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.GooglePasswordManager.NavigationResult", result);
}

void RecordSuccessOrFailure(NavigationResult result,
                            base::TimeTicks navigation_start) {
  RecordNavigationResult(result);
  switch (result) {
    case NavigationResult::kSuccess:
      UMA_HISTOGRAM_TIMES("PasswordManager.GooglePasswordManager.TimeToSuccess",
                          base::TimeTicks::Now() - navigation_start);
      return;
    case NavigationResult::kFailure:
      UMA_HISTOGRAM_TIMES("PasswordManager.GooglePasswordManager.TimeToFailure",
                          base::TimeTicks::Now() - navigation_start);
      return;
    case NavigationResult::kTimeout:
      break;
  }

  NOTREACHED();
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
GooglePasswordManagerNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  content::WebContents* web_contents = handle->GetWebContents();
  // Don't create a throttle if the user is not navigating to the Google
  // Password Manager.
  if (handle->GetURL().GetOrigin() != GURL(chrome::kGooglePasswordManagerURL))
    return nullptr;

  // Also don't create a throttle if the user should not manage their passwords
  // in the Google Password Manager (e.g. the user is not syncing passwords).
  if (!ShouldManagePasswordsinGooglePasswordManager(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    return nullptr;
  }

  // Lastly, don't create a throttle if the navigation does not originate from
  // clicking a link (e.g. the user entered the URL in the omnibar).
  if (!ui::PageTransitionCoreTypeIs(handle->GetPageTransition(),
                                    ui::PAGE_TRANSITION_LINK)) {
    return nullptr;
  }

  return std::make_unique<GooglePasswordManagerNavigationThrottle>(handle);
}

GooglePasswordManagerNavigationThrottle::
    ~GooglePasswordManagerNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillStartRequest() {
  timer_.Start(FROM_HERE, kTimeout, this,
               &GooglePasswordManagerNavigationThrottle::OnTimeout);
  navigation_start_ = base::TimeTicks::Now();
  return NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillFailRequest() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    RecordSuccessOrFailure(NavigationResult::kFailure, navigation_start_);
    NavigateToChromeSettingsPasswordsPage(navigation_handle());
  }
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

content::NavigationThrottle::ThrottleCheckResult
GooglePasswordManagerNavigationThrottle::WillProcessResponse() {
  if (timer_.IsRunning()) {
    timer_.Stop();
    RecordSuccessOrFailure(NavigationResult::kSuccess, navigation_start_);
    return content::NavigationThrottle::PROCEED;
  }

  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

const char* GooglePasswordManagerNavigationThrottle::GetNameForLogging() {
  return "GooglePasswordManagerNavigationThrottle";
}

void GooglePasswordManagerNavigationThrottle::OnTimeout() {
  RecordNavigationResult(NavigationResult::kTimeout);
  NavigateToChromeSettingsPasswordsPage(navigation_handle());
}
