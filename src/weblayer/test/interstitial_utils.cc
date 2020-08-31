// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/interstitial_utils.h"

#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

namespace {

// Returns the security interstitial currently showing in |tab|, or null if
// there is no such interstitial.
security_interstitials::SecurityInterstitialPage*
GetCurrentlyShowingInterstitial(Tab* tab) {
  TabImpl* tab_impl = static_cast<TabImpl*>(tab);

  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab_impl->web_contents());

  return helper
             ? helper
                   ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
             : nullptr;
}

// Returns true if a security interstitial of type |type| is currently showing
// in |tab|.
bool IsShowingInterstitialOfType(
    Tab* tab,
    security_interstitials::SecurityInterstitialPage::TypeID type) {
  auto* blocking_page = GetCurrentlyShowingInterstitial(tab);

  if (!blocking_page)
    return false;

  return blocking_page->GetTypeForTesting() == type;
}

}  // namespace

bool IsShowingSecurityInterstitial(Tab* tab) {
  return GetCurrentlyShowingInterstitial(tab) != nullptr;
}

bool IsShowingSSLInterstitial(Tab* tab) {
  return IsShowingInterstitialOfType(tab, SSLBlockingPage::kTypeForTesting);
}

bool IsShowingCaptivePortalInterstitial(Tab* tab) {
  return IsShowingInterstitialOfType(
      tab, CaptivePortalBlockingPage::kTypeForTesting);
}

bool IsShowingBadClockInterstitial(Tab* tab) {
  return IsShowingInterstitialOfType(tab,
                                     BadClockBlockingPage::kTypeForTesting);
}

}  // namespace weblayer
