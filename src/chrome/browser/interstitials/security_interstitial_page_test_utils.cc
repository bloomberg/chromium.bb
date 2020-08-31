// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"

#include "base/strings/stringprintf.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace chrome_browser_interstitials {

bool IsInterstitialDisplayingText(content::RenderFrameHost* interstitial_frame,
                                  const std::string& text) {
  // It's valid for |text| to contain "\'", but simply look for "'" instead
  // since this function is used for searching host names and a predefined
  // string.
  DCHECK(text.find("\'") == std::string::npos);
  std::string command = base::StringPrintf(
      "var hasText = document.body.textContent.indexOf('%s') >= 0;"
      "window.domAutomationController.send(hasText ? %d : %d);",
      text.c_str(), security_interstitials::CMD_TEXT_FOUND,
      security_interstitials::CMD_TEXT_NOT_FOUND);
  int result = 0;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(interstitial_frame, command,
                                                  &result));
  return result == security_interstitials::CMD_TEXT_FOUND;
}

bool InterstitialHasProceedLink(content::RenderFrameHost* interstitial_frame) {
  int result = security_interstitials::CMD_ERROR;
  const std::string javascript = base::StringPrintf(
      "domAutomationController.send("
      "(document.querySelector(\"#proceed-link\") === null) "
      "? (%d) : (%d))",
      security_interstitials::CMD_TEXT_NOT_FOUND,
      security_interstitials::CMD_TEXT_FOUND);
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(interstitial_frame,
                                                  javascript, &result));
  return result == security_interstitials::CMD_TEXT_FOUND;
}

bool IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  return helper &&
         helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
}

bool IsShowingCaptivePortalInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(), "Connect to");
}

bool IsShowingSSLInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(),
                                      "Your connection is not private");
}

bool IsShowingMITMInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(),
                                      "An application is stopping");
}

bool IsShowingBadClockInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(), "Your clock is");
}

bool IsShowingBlockedInterceptionInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(),
                                      "Anything you type, any pages you view");
}

bool IsShowingLegacyTLSInterstitial(content::WebContents* tab) {
  return IsShowingInterstitial(tab) &&
         IsInterstitialDisplayingText(tab->GetMainFrame(),
                                      "outdated security configuration");
}

}  // namespace chrome_browser_interstitials
