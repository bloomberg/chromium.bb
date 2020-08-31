// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/ios_security_interstitial_page.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "ios/components/ui_util/dynamic_type_util.h"
#include "ios/web/common/features.h"
#include "ios/web/public/security/web_interstitial.h"
#import "ios/web/public/web_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace security_interstitials {

namespace {
// Adjusts the interstitial page's template parameter "fontsize" by system font
// size multiplier.
void AdjustFontSize(base::DictionaryValue& load_time_data) {
  base::Value* value = load_time_data.FindKey("fontsize");
  DCHECK(value && value->is_string());
  std::string old_size = value->GetString();
  // |old_size| should be in form of "75%".
  DCHECK(old_size.size() > 1 && old_size.back() == '%');
  double new_size = 75.0;
  bool converted =
      base::StringToDouble(old_size.substr(0, old_size.size() - 1), &new_size);
  DCHECK(converted);
  new_size *= ui_util::SystemSuggestedFontSizeMultiplier();
  load_time_data.SetString("fontsize", base::StringPrintf("%.0lf%%", new_size));
}
}  // namespace

IOSSecurityInterstitialPage::IOSSecurityInterstitialPage(
    web::WebState* web_state,
    const GURL& request_url,
    IOSBlockingPageControllerClient* client)
    : web_state_(web_state),
      request_url_(request_url),
      web_interstitial_(nullptr),
      client_(client) {
  // Creating web_interstitial_ without showing it leaks memory, so don't
  // create it here.
}

IOSSecurityInterstitialPage::~IOSSecurityInterstitialPage() {}

void IOSSecurityInterstitialPage::Show() {
  DCHECK(!web_interstitial_);
  web_interstitial_ = web::WebInterstitial::CreateInterstitial(
      web_state_, ShouldCreateNewNavigation(), request_url_,
      std::unique_ptr<web::WebInterstitialDelegate>(this));
  web_interstitial_->Show();
  AfterShow();
}

std::string IOSSecurityInterstitialPage::GetHtmlContents() const {
  base::DictionaryValue load_time_data;
  load_time_data.SetBoolean(
      "committed_interstitials_enabled",
      base::FeatureList::IsEnabled(web::features::kSSLCommittedInterstitials));
  // Interstitial pages on iOS get reloaded to prevent loading from cache, since
  // loading from cache breaks JavaScript commands. Set as |load_time_data|
  // for safety.
  load_time_data.SetString("url_to_reload", request_url_.spec());
  PopulateInterstitialStrings(&load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  AdjustFontSize(load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SECURITY_INTERSTITIAL_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}

base::string16 IOSSecurityInterstitialPage::GetFormattedHostName() const {
  return security_interstitials::common_string_util::GetFormattedHostName(
      request_url_);
}

}  // namespace security_interstitials
