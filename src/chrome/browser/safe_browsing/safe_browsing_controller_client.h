// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_CONTROLLER_CLIENT_H_

#include "base/macros.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"

namespace content {
class WebContents;
}

class PrefService;

// Provides embedder-specific logic for the Safe Browsing interstitial page
// controller.
class SafeBrowsingControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  SafeBrowsingControllerClient(
      content::WebContents* web_contents,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      PrefService* prefs,
      const std::string& app_locale,
      const GURL& default_safe_page);
  ~SafeBrowsingControllerClient() override;

  void Proceed() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingControllerClient);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_CONTROLLER_CLIENT_H_
