// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/safe_browsing_controller_client.h"

#include "base/feature_list.h"
#include "components/safe_browsing/core/features.h"
#include "components/security_interstitials/core/metrics_helper.h"

namespace safe_browsing {

SafeBrowsingControllerClient::SafeBrowsingControllerClient(
    content::WebContents* web_contents,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    PrefService* prefs,
    const std::string& app_locale,
    const GURL& default_safe_page)
    : security_interstitials::SecurityInterstitialControllerClient(
          web_contents,
          std::move(metrics_helper),
          prefs,
          app_locale,
          default_safe_page) {}

SafeBrowsingControllerClient::~SafeBrowsingControllerClient() {}

void SafeBrowsingControllerClient::Proceed() {
  // With committed interstitials the site has already
  // been added to the whitelist, so reload will proceed.
  Reload();
  return;
}

void SafeBrowsingControllerClient::GoBack() {
  SecurityInterstitialControllerClient::GoBackAfterNavigationCommitted();
  return;
}

}  // namespace safe_browsing
