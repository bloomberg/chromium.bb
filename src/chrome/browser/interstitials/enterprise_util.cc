// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/enterprise_util.h"

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
extensions::SafeBrowsingPrivateEventRouter* GetEventRouter(
    content::WebContents* web_contents) {
  // |web_contents| can be null in tests.
  if (!web_contents)
    return nullptr;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (browser_context->IsOffTheRecord())
    return nullptr;

  return extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
      browser_context);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

void MaybeTriggerSecurityInterstitialShownEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetEventRouter(web_contents);
  if (!event_router)
    return;
  event_router->OnSecurityInterstitialShown(page_url, reason, net_error_code);
#endif
}

void MaybeTriggerSecurityInterstitialProceededEvent(
    content::WebContents* web_contents,
    const GURL& page_url,
    const std::string& reason,
    int net_error_code) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SafeBrowsingPrivateEventRouter* event_router =
      GetEventRouter(web_contents);
  if (!event_router)
    return;
  event_router->OnSecurityInterstitialProceeded(page_url, reason,
                                                net_error_code);
#endif
}
