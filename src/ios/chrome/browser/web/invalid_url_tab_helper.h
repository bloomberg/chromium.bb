// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_INVALID_URL_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_INVALID_URL_TAB_HELPER_H_

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// A tab helper that cancels the navigation to invalid URLs:
//  - Invalid data URLs (these request download if the load is allowed,
//    which is suboptimal user experience).
//  - Extremely long URLs (these use a lot of memory during the navigation
//    causing stability problems).
class InvalidUrlTabHelper : public web::WebStateUserData<InvalidUrlTabHelper>,
                            public web::WebStatePolicyDecider {
 public:
  ~InvalidUrlTabHelper() override;

  InvalidUrlTabHelper(const InvalidUrlTabHelper&) = delete;
  InvalidUrlTabHelper& operator=(const InvalidUrlTabHelper&) = delete;

 private:
  explicit InvalidUrlTabHelper(web::WebState* web_state);
  PolicyDecision ShouldAllowRequest(NSURLRequest* request,
                                    const RequestInfo& request_info) override;

  friend class web::WebStateUserData<InvalidUrlTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_INVALID_URL_TAB_HELPER_H_
