// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_captive_portal_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_metrics.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSCaptivePortalBlockingPage::IOSCaptivePortalBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    const GURL& landing_url,
    security_interstitials::IOSBlockingPageControllerClient* client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client),
      landing_url_(landing_url) {
  captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
      captive_portal::CaptivePortalMetrics::SHOW_ALL);
}

IOSCaptivePortalBlockingPage::~IOSCaptivePortalBlockingPage() {}

bool IOSCaptivePortalBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void IOSCaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::Value* load_time_data) const {
  load_time_data->SetStringKey("iconClass", "icon-offline");
  load_time_data->SetStringKey("type", "CAPTIVE_PORTAL");
  load_time_data->SetBoolKey("overridable", false);
  load_time_data->SetStringKey(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));

  std::u16string tab_title =
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_HEADING_WIFI);
  load_time_data->SetStringKey("tabTitle", tab_title);
  load_time_data->SetStringKey("heading", tab_title);

  std::u16string paragraph;
  if (landing_url_.spec() ==
      captive_portal::CaptivePortalDetector::kDefaultURL) {
    // Captive portal may intercept requests without HTTP redirects, in which
    // case the login url would be the same as the captive portal detection url.
    // Don't show the login url in that case.
    paragraph = l10n_util::GetStringUTF16(
        IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI);
  } else {
    // Portal redirection was done with HTTP redirects, so show the login URL.
    // If |languages| is empty, punycode in |login_host| will always be decoded.
    std::u16string login_host =
        url_formatter::IDNToUnicode(landing_url_.host());
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    paragraph = l10n_util::GetStringFUTF16(
        IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI, login_host);
  }
  load_time_data->SetStringKey("primaryParagraph", paragraph);
  // Explicitly specify other expected fields to empty.
  load_time_data->SetStringKey("openDetails", std::u16string());
  load_time_data->SetStringKey("closeDetails", std::u16string());
  load_time_data->SetStringKey("explanationParagraph", std::u16string());
  load_time_data->SetStringKey("finalParagraph", std::u16string());
  load_time_data->SetStringKey("recurrentErrorParagraph", std::u16string());
  load_time_data->SetStringKey("optInLink", std::u16string());
  load_time_data->SetStringKey("enhancedProtectionMessage", std::u16string());
  load_time_data->SetBoolKey("show_recurrent_error_paragraph", false);
}

void IOSCaptivePortalBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  // Any command other than "open the login page" is ignored.
  if (command == security_interstitials::CMD_OPEN_LOGIN) {
    captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
        captive_portal::CaptivePortalMetrics::OPEN_LOGIN_PAGE);

    CaptivePortalDetectorTabHelper::FromWebState(web_state())
        ->DisplayCaptivePortalLoginPage(landing_url_);
  }
}
