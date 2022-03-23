// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_

#include "ios/components/security_interstitials/https_only_mode/https_only_mode_controller_client.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that is
// shown on an HTTP URL in HTTPS-Only mode.
class HttpsOnlyModeBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  HttpsOnlyModeBlockingPage(const HttpsOnlyModeBlockingPage&) = delete;
  HttpsOnlyModeBlockingPage& operator=(const HttpsOnlyModeBlockingPage&) =
      delete;

  ~HttpsOnlyModeBlockingPage() override;

  // Creates an HTTPS only mode blocking page.
  HttpsOnlyModeBlockingPage(
      web::WebState* web_state,
      const GURL& request_url,
      std::unique_ptr<HttpsOnlyModeControllerClient> client);

 protected:
  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(base::Value* load_time_data) const override;
  bool ShouldDisplayURL() const override;

 private:
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command,
      const GURL& origin_url,
      bool user_is_interacting,
      web::WebFrame* sender_frame) override;

  web::WebState* web_state_ = nullptr;
  std::unique_ptr<HttpsOnlyModeControllerClient> controller_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_BLOCKING_PAGE_H_
