// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_IMPL_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_IMPL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"

class WebStateListWebUsageEnablerImpl : public WebStateListWebUsageEnabler,
                                        public WebStateListObserver {
 public:
  WebStateListWebUsageEnablerImpl();

  // WebStateListWebUsageEnabler:
  void SetWebStateList(WebStateList* web_state_list) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool web_usage_enabled) override;
  bool TriggersInitialLoad() const override;
  void SetTriggersInitialLoad(bool triggers_initial_load) override;

 private:
  // Updates the web usage enabled status of all WebStates in |web_state_list_|
  // to |web_usage_enabled_|.
  void UpdateWebUsageEnabled();
  // Called when |web_state| is added to |web_state_list_| to updates its web
  // usage enabled state, optionally kicking off its initial load.
  void UpdateWebUsageForAddedWebState(web::WebState* web_state);

  // KeyedService:
  void Shutdown() override;

  // WebStateListObserver:
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;
  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override;

  // The WebStateList whose WebStates' web usage is being managed.
  WebStateList* web_state_list_ = nullptr;
  // Whether web usage is enabled for the WebState in |web_state_list_|.
  bool web_usage_enabled_ = false;
  // Whether the initial load for a WebState added to |web_state_list_| should
  // be triggered if |web_usage_enabled_| is true.
  bool triggers_initial_load_ = true;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_WEB_USAGE_ENABLER_WEB_STATE_LIST_WEB_USAGE_ENABLER_IMPL_H_
