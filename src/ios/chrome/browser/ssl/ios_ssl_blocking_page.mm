// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#include "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/web_state.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::SSLErrorOptionsMask;
using security_interstitials::SSLErrorUI;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
IOSSSLBlockingPage::IOSSSLBlockingPage(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
        client)
    : IOSSecurityInterstitialPage(web_state, request_url, client.get()),
      web_state_(web_state),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(options_mask)),
      controller_(std::move(client)) {
  DCHECK(web_state_);
  // Override prefs for the SSLErrorUI.
  if (overridable_)
    options_mask |= SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  else
    options_mask &= ~SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;

  ssl_error_ui_.reset(new SSLErrorUI(request_url, cert_error, ssl_info,
                                     options_mask, time_triggered, GURL(),
                                     controller_.get()));

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  safe_browsing::SafeBrowsingMetricsCollector* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForBrowserState(browser_state);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_SSL_INTERSTITIAL);
  }

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool IOSSSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

IOSSSLBlockingPage::~IOSSSLBlockingPage() {
}

void IOSSSLBlockingPage::PopulateInterstitialStrings(
    base::Value* load_time_data) const {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
}

// static
bool IOSSSLBlockingPage::IsOverridable(int options_mask) {
  return (options_mask & SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
         !(options_mask & SSLErrorOptionsMask::STRICT_ENFORCEMENT);
}

void IOSSSLBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  // If a proceed command is received, allowlist the certificate and reload
  // the page to re-initiate the original navigation.
  if (command == security_interstitials::CMD_PROCEED) {
    web_state_->GetSessionCertificatePolicyCache()->RegisterAllowedCertificate(
        ssl_info_.cert, request_url().host(), ssl_info_.cert_status);
    web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                               /*check_for_repost=*/true);
    return;
  }

  ssl_error_ui_->HandleCommand(command);
}
