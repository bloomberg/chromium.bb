// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_

#include <string>

#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace web {
class WebFrame;
class WebState;
}  // namespace web

namespace security_interstitials {

class IOSSecurityInterstitialPage {
 public:
  IOSSecurityInterstitialPage(web::WebState* web_state,
                              const GURL& request_url,
                              IOSBlockingPageControllerClient* client);

  IOSSecurityInterstitialPage(const IOSSecurityInterstitialPage&) = delete;
  IOSSecurityInterstitialPage& operator=(const IOSSecurityInterstitialPage&) =
      delete;

  virtual ~IOSSecurityInterstitialPage();

  // Returns the HTML that should be displayed in the page
  virtual std::string GetHtmlContents() const;

  // Whether a URL should be displayed on this interstitial page. This is
  // respected by committed interstitials only.
  virtual bool ShouldDisplayURL() const;

  // Handles `command` from the interstitial page. Overridden in subclasses
  // to handle actions specific to the type of interstitial.
  virtual void HandleCommand(SecurityInterstitialCommand command,
                             const GURL& origin_url,
                             bool user_is_interacting,
                             web::WebFrame* sender_frame) = 0;

 protected:
  // Returns true if the interstitial should create a new navigation item.
  virtual bool ShouldCreateNewNavigation() const = 0;

  // Populates the strings used to generate the HTML from the template.
  virtual void PopulateInterstitialStrings(
      base::Value* load_time_data) const = 0;

  // Returns the formatted host name for the request url.
  std::u16string GetFormattedHostName() const;

  web::WebState* web_state() const { return web_state_; }
  const GURL& request_url() const { return request_url_; }

 private:
  // The WebState with which this interstitial page is associated. Not
  // available in the destructor since the it can be destroyed before this
  // class is destroyed.
  web::WebState* web_state_;
  const GURL request_url_;

  // Used to interact with the embedder. Unowned pointer; must outlive |this|
  // instance.
  IOSBlockingPageControllerClient* const client_ = nullptr;
};

}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_
