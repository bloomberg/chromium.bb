// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_
#define COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_

#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"
#include "ui/base/page_transition_types.h"

namespace web {
class WebState;
class NavigationContext;
}  // namespace web

namespace url_param_filter {

// This class is created to measure the effect of experimentally filtering
// URLs. It is only attached to WebStates created via an "Open In Incognito"
// press.
//
// The state-machine logic measuring refreshes in class should be kept in sync
// with the CrossOtrObserver at components/url_param_filter/content/ which
// performs similar observations.
class CrossOtrTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<CrossOtrTabHelper> {
 public:
  ~CrossOtrTabHelper() override;

  // Attaches the observer in cases where it should do so; leaves `web_state`
  // unchanged otherwise.
  static void CreateForWebState(web::WebState* web_state);

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Returns whether this observer is in Cross-OTR state, used for testing.
  bool GetCrossOtrStateForTesting() const;

 private:
  CrossOtrTabHelper(web::WebState* web_state);
  friend class WebStateUserData<CrossOtrTabHelper>;

  // Flushes metrics and removes the observer from the WebState.
  void Detach(web::WebState* web_state);

  // Drives state machine logic; we write the cross-OTR response code metric
  // only for the first navigation, which is that which would have parameters
  // filtered.
  bool observed_response_ = false;

  // Tracks refreshes observed, which could point to an issue with param
  // filtering causing unexpected behavior for the user.
  int refresh_count_ = 0;

  // Whether top-level navigations should have filtering applied. Starts true
  // then switches to false once a navigation completes and then either:
  // user interaction is observed or a new navigation starts that is not a
  // client redirect.
  bool protecting_navigations_ = true;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace url_param_filter

#endif  // COMPONENTS_URL_PARAM_FILTER_IOS_CROSS_OTR_TAB_HELPER_H_
